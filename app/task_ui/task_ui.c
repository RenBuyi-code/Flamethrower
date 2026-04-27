/**
 * @file    task_ui.c
 * @brief   UI任务实现
 *
 * UI任务模块，负责：
 *   - 初始化和管理UI界面
 *   - 处理用户输入（按键）
 *   - 显示系统状态和参数
 *   - 处理设置页面的参数修改
 *   - 与SlateUI库集成，管理页面切换
 *
 * 设计思路：
 *   - 使用SlateUI库管理页面
 *   - 使用MultiButton库处理按键输入
 *   - 定期更新系统状态到UI
 *   - 实现参数修改和保存功能
 *   - 与其他模块的关系：
 *     - app_fsm：获取系统状态和参数
 *     - task_control：通过menu_active标志通知控制任务
 *     - SlateUI：使用其页面管理和显示功能
 */

#include "task_ui.h"
#include "../ui_services.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../rules/dmx_strategy.h"
#include "../../middleware/MultiButton/multi_button.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/core/inc/sl_page_registry.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/port/sl_port.h"
#include "../../app/ui_pages/ui_idle_page.h"
#include "../../app/ui_pages/ui_main_menu.h"
#include "../../app/ui_pages/ui_safety_page.h"
#include "../../app/ui_pages/ui_setting_page.h"
#include "../../app/ui_pages/ui_language_page.h"
#include "../../app/ui_pages/ui_splash_page.h"
#include "../../app/ui_pages/ui_checking_page.h"
#include <string.h>

/** @brief 按键ID定义 */
enum
{
  UI_BTN_ID_MENU = 1,    /**< 菜单键ID */
  UI_BTN_ID_DOWN = 2,    /**< 下键ID */
  UI_BTN_ID_UP = 3,      /**< 上键ID */
  UI_BTN_ID_ENTER = 4    /**< 确认键ID */
};

/**
 * @brief   UI按钮上下文结构体
 *
 * 管理所有按键的状态和配置
 */
typedef struct
{
  /** @brief 菜单键 */
  Button key_menu;
  /** @brief 下键 */
  Button key_down;
  /** @brief 上键 */
  Button key_up;
  /** @brief 确认键 */
  Button key_enter;
  /** @brief 初始化标志 */
  bool initialized;
} ui_button_ctx_t;

typedef struct
{
  machine_state_t state;
  uint8_t pressure_pct;
  bool dmx_online;
  bool pumping;
} ui_runtime_sample_t;

/** @brief UI任务全局配置（静态单例） */
static app_task_ui_cfg_t s_task_ui_cfg;
/** @brief UI按钮上下文 */
static ui_button_ctx_t s_ui_btn;
/** @brief DMX地址草稿值（用于设置页面） */
static int16_t s_draft_dmx_addr;
/** @brief DMX模式草稿值（用于设置页面） */
static int16_t s_draft_dmx_mode;
/** @brief 点火延迟草稿值（用于设置页面） */
static int16_t s_draft_ign_delay;
/** @brief 锁定延迟草稿值（用于设置页面） */
static int16_t s_draft_lock_delay;
/** @brief 倾斜保护启用草稿值（用于设置页面） */
static int16_t s_draft_tilt_enable;
/** @brief 语言草稿值（用于设置页面） */
static int16_t s_draft_language;
/** @brief 页面注册标志 */
static bool s_ui_pages_registered;

/**
 * @brief   UI页面定义
 *
 * 定义所有可用的UI页面
 */
static const sl_PageEntry s_ui_pages[] =
{
  { "splash", ui_splash_page_get },        /**< 启动页面 */
  { "checking", ui_checking_page_get },    /**< 检查页面 */
  { "idle", ui_idle_page_get },            /**< 空闲页面 */
  { "main_menu", ui_main_menu_get },       /**< 主菜单页面 */
  { "dmx_set", ui_setting_page_get_dmx },  /**< DMX设置页面 */
  { "pressure_set", ui_setting_page_get_pressure },  /**< 压力设置页面 */
  { "safety", ui_safety_page_get },        /**< 安全页面 */
  { "language", ui_language_page_get }     /**< 语言设置页面 */
};

static bool task_ui_apply_params_update(void (*mutator)(system_params_t *params), const char *log_key, uint32_t log_val)
{
  system_params_t params;

  if((s_task_ui_cfg.app == 0) || (mutator == 0))
  {
    return false;
  }

  if(app_fsm_get_params_snapshot(s_task_ui_cfg.app, &params) == false)
  {
    return false;
  }

  mutator(&params);
  if(app_fsm_apply_params(s_task_ui_cfg.app, &params) == false)
  {
    APP_LOGW("%s apply failed", log_key);
    return false;
  }

  APP_LOGI("%s=%u", log_key, (unsigned)log_val);
  return true;
}

static void task_ui_mutate_dmx_addr(system_params_t *params)
{
  params->dmx_address = (uint16_t)s_draft_dmx_addr;
}

static void task_ui_mutate_dmx_mode(system_params_t *params)
{
  params->dmx_mode = (s_draft_dmx_mode == 1) ? DMX_MODE_6CH : DMX_MODE_2CH;
}

static void task_ui_mutate_ign_delay(system_params_t *params)
{
  params->igniter_delay_ms = (uint16_t)s_draft_ign_delay;
}

static void task_ui_mutate_lock_delay(system_params_t *params)
{
  params->oil_lock_delay_ms = (uint16_t)s_draft_lock_delay;
}

static void task_ui_mutate_tilt_enable(system_params_t *params)
{
  params->tilt_protect_enable = (s_draft_tilt_enable != 0) ? true : false;
}

static void task_ui_mutate_language(system_params_t *params)
{
  params->language = (uint8_t)s_draft_language;
}

/**
 * @brief   保存DMX地址
 *
 * @param[in] value  DMX地址值
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_dmx_addr(int16_t value)
{
  s_draft_dmx_addr = value;
  (void)task_ui_apply_params_update(task_ui_mutate_dmx_addr, "dmx addr", (uint32_t)(uint16_t)value);
}

/**
 * @brief   保存DMX模式
 *
 * @param[in] value  DMX模式值（0=2CH, 1=6CH）
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_dmx_mode(int16_t value)
{
  s_draft_dmx_mode = value;
  (void)task_ui_apply_params_update(task_ui_mutate_dmx_mode, "dmx mode", (uint32_t)((value == 1) ? DMX_MODE_6CH : DMX_MODE_2CH));
}

/**
 * @brief   保存点火延迟
 *
 * @param[in] value  点火延迟值（毫秒）
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_ign_delay(int16_t value)
{
  s_draft_ign_delay = value;
  (void)task_ui_apply_params_update(task_ui_mutate_ign_delay, "ign delay", (uint32_t)(uint16_t)value);
}

/**
 * @brief   保存锁定延迟
 *
 * @param[in] value  锁定延迟值（毫秒）
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_lock_delay(int16_t value)
{
  s_draft_lock_delay = value;
  (void)task_ui_apply_params_update(task_ui_mutate_lock_delay, "lock delay", (uint32_t)(uint16_t)value);
}

/**
 * @brief   保存倾斜保护启用状态
 *
 * @param[in] value  倾斜保护启用状态（0=禁用, 非0=启用）
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_tilt_enable(int16_t value)
{
  s_draft_tilt_enable = value;
  (void)task_ui_apply_params_update(task_ui_mutate_tilt_enable, "tilt protect", (uint32_t)((value != 0) ? 1U : 0U));
}

/**
 * @brief   保存语言设置
 *
 * @param[in] value  语言代码
 *
 * 操作流程：
 *   1. 更新系统参数
 *   2. 输出日志
 *   3. 提交参数修改
 */
static void task_ui_save_language(int16_t value)
{
  s_draft_language = value;
  (void)task_ui_apply_params_update(task_ui_mutate_language, "language", (uint32_t)(uint8_t)value);
}

/**
 * @brief   读取按键状态
 *
 * @param[in] button_id  按键ID
 * @return    按键状态（1=按下, 0=释放）
 *
 * 从硬件读取按键状态
 */
static uint8_t task_ui_button_level_read(uint8_t button_id)
{
  switch(button_id)
  {
    case UI_BTN_ID_MENU:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_MENU) ? 1U : 0U;
    case UI_BTN_ID_DOWN:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_DOWN) ? 1U : 0U;
    case UI_BTN_ID_UP:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_UP) ? 1U : 0U;
    case UI_BTN_ID_ENTER:
      return s_task_ui_cfg.app->hal.input.read(s_task_ui_cfg.app->hal.input.ctx, INPUT_KEY_ENTER) ? 1U : 0U;
    default:
      return 0U;
  }
}

/**
 * @brief   注册UI页面（仅执行一次）
 *
 * 操作流程：
 *   1. 重置页面注册表
 *   2. 注册所有页面
 *   3. 设置注册标志
 */
static void task_ui_register_pages_once(void)
{
  if(s_ui_pages_registered)
  {
    return;
  }

  sl_ui_registry_reset();
  s_ui_pages_registered = sl_ui_register_pages(s_ui_pages, (uint8_t)(sizeof(s_ui_pages) / sizeof(s_ui_pages[0])));
}

/**
 * @brief   按键点击回调函数
 *
 * @param[in] btn         按键实例
 * @param[in] user_data   用户数据（未使用）
 *
 * 处理按键点击事件，将其转换为SlateUI按键事件
 */
static void task_ui_btn_click_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  APP_LOGI("key btn_id=%u", (unsigned)btn->button_id);

  switch(btn->button_id)
  {
    case UI_BTN_ID_MENU:
      sl_ui_post_key(SL_UI_KEY_BACK, true);
      break;
    case UI_BTN_ID_DOWN:
      sl_ui_post_key(SL_UI_KEY_DOWN, true);
      break;
    case UI_BTN_ID_UP:
      sl_ui_post_key(SL_UI_KEY_UP, true);
      break;
    case UI_BTN_ID_ENTER:
      sl_ui_post_key(SL_UI_KEY_ENTER, true);
      break;
    default:
      break;
  }
}

/**
 * @brief   按键长按重复回调函数
 *
 * @param[in] btn         按键实例
 * @param[in] user_data   用户数据（未使用）
 *
 * 处理按键长按重复事件，用于快速调整数值
 */
static void task_ui_btn_repeat_cb(Button *btn, void *user_data)
{
  (void)user_data;
  if(btn == 0)
  {
    return;
  }

  if(btn->button_id == UI_BTN_ID_DOWN)
  {
    sl_ui_post_key(SL_UI_KEY_DOWN, true);
  }
  else if(btn->button_id == UI_BTN_ID_UP)
  {
    sl_ui_post_key(SL_UI_KEY_UP, true);
  }
}

static void task_ui_init_draft_params(void)
{
  s_draft_dmx_addr = (int16_t)s_task_ui_cfg.app->params.dmx_address;
  s_draft_dmx_mode = (int16_t)(s_task_ui_cfg.app->params.dmx_mode == DMX_MODE_6CH ? 1 : 0);
  s_draft_ign_delay = (int16_t)s_task_ui_cfg.app->params.igniter_delay_ms;
  s_draft_lock_delay = (int16_t)s_task_ui_cfg.app->params.oil_lock_delay_ms;
  s_draft_tilt_enable = s_task_ui_cfg.app->params.tilt_protect_enable ? 1 : 0;
  s_draft_language = (int16_t)s_task_ui_cfg.app->params.language;
}

static void task_ui_bind_page_refs(void)
{
  ui_setting_page_set_dmx_refs(&s_draft_dmx_addr, &s_draft_dmx_mode);
  ui_setting_page_set_pressure_refs(&s_draft_ign_delay, &s_draft_lock_delay);
  ui_safety_page_set_tilt_ref(&s_draft_tilt_enable);
}

static void task_ui_bind_setting_handlers(void)
{
  ui_setting_handlers_t handlers;

  handlers.save_dmx_addr = task_ui_save_dmx_addr;
  handlers.save_dmx_mode = task_ui_save_dmx_mode;
  handlers.save_ign_delay = task_ui_save_ign_delay;
  handlers.save_lock_delay = task_ui_save_lock_delay;
  handlers.save_tilt_enable = task_ui_save_tilt_enable;
  handlers.save_language = task_ui_save_language;
  ui_service_bind_setting_handlers(&handlers);
}

static void task_ui_log_initial_button_state(void)
{
  uint8_t m;
  uint8_t d;
  uint8_t u;
  uint8_t e;

  m = task_ui_button_level_read(UI_BTN_ID_MENU);
  d = task_ui_button_level_read(UI_BTN_ID_DOWN);
  u = task_ui_button_level_read(UI_BTN_ID_UP);
  e = task_ui_button_level_read(UI_BTN_ID_ENTER);
  APP_LOGI("btn init state M=%u D=%u U=%u E=%u", (unsigned)m, (unsigned)d, (unsigned)u, (unsigned)e);
}

static void task_ui_init_buttons(void)
{
  button_init(&s_ui_btn.key_menu, task_ui_button_level_read, 1U, UI_BTN_ID_MENU);
  button_init(&s_ui_btn.key_down, task_ui_button_level_read, 1U, UI_BTN_ID_DOWN);
  button_init(&s_ui_btn.key_up, task_ui_button_level_read, 1U, UI_BTN_ID_UP);
  button_init(&s_ui_btn.key_enter, task_ui_button_level_read, 1U, UI_BTN_ID_ENTER);

  button_attach(&s_ui_btn.key_menu, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_enter, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);

  (void)button_start(&s_ui_btn.key_menu);
  (void)button_start(&s_ui_btn.key_down);
  (void)button_start(&s_ui_btn.key_up);
  (void)button_start(&s_ui_btn.key_enter);
}

static void task_ui_init_display_and_pages(void)
{
  sl_lang_set((int)s_task_ui_cfg.app->params.language);
  sl_disp_init();
  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  sl_disp_flush();
  task_ui_register_pages_once();
  sl_ui_init("splash");
}

/**
 * @brief   UI任务初始化（仅执行一次）
 *
 * 操作流程：
 *   1. 初始化草稿参数值
 *   2. 设置页面引用
 *   3. 绑定设置页面回调函数
 *   4. 初始化SlateUI和端口
 *   5. 初始化按键
 *   6. 注册UI页面
 *   7. 初始化SlateUI
 */
static void task_ui_setup_once(void)
{
  if((s_task_ui_cfg.app == 0) || s_ui_btn.initialized)
  {
    return;
  }

  task_ui_init_draft_params();
  task_ui_bind_page_refs();
  task_ui_bind_setting_handlers();

  /** 初始化SlateUI和端口 */
  sl_port_init();
  sl_port_input_init();

  task_ui_log_initial_button_state();
  task_ui_init_buttons();
  task_ui_init_display_and_pages();

  s_ui_btn.initialized = true;
  APP_LOGI("ui setup done");
}


/* UI page classification helper for menu-active status. */
static bool task_ui_is_menu_page(const char *page_name)
{
  return ((page_name != 0) &&
          (strcmp(page_name, "idle") != 0) &&
          (strcmp(page_name, "checking") != 0) &&
          (strcmp(page_name, "splash") != 0));
}

static void task_ui_update_menu_active_flag(const char *page_name)
{
  if(s_task_ui_cfg.menu_active != 0)
  {
    *s_task_ui_cfg.menu_active = task_ui_is_menu_page(page_name);
  }
}

static void task_ui_heartbeat_delay(void)
{
  (void)xEventGroupSetBits(s_task_ui_cfg.event_group, s_task_ui_cfg.hb_bit);
  vTaskDelay(pdMS_TO_TICKS(TICKS_INTERVAL));
}

static void task_ui_collect_runtime(ui_runtime_sample_t *sample)
{
  EventBits_t bits;
  uint16_t pressure_raw;
  actuator_status_t act_st;
  BaseType_t act_ok;

  bits = xEventGroupGetBits(s_task_ui_cfg.event_group);
  pressure_raw = s_task_ui_cfg.app->hal.adc.read_raw(s_task_ui_cfg.app->hal.adc.ctx, SENSOR_PRESSURE);
  sample->pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
  sample->dmx_online = ((bits & s_task_ui_cfg.dmx_online_bit) != 0U);
  sample->state = s_task_ui_cfg.app->machine.current;

  act_ok = xQueuePeek(s_task_ui_cfg.q_actuator_status, &act_st, 0);
  sample->pumping = ((act_ok == pdTRUE) &&
                     act_st.out.oil_pump_on &&
                     (act_st.fire_active == false) &&
                     (act_st.relief_active == false));
}

static void task_ui_update_snapshot_and_redraw(const ui_runtime_sample_t *sample)
{
  ui_machine_snapshot_t snap;
  bool snapshot_changed;

  snap.state = sample->state;
  snap.pressure_pct = sample->pressure_pct;
  snap.dmx_addr = s_task_ui_cfg.app->params.dmx_address;
  snap.fault_mask = s_task_ui_cfg.app->faults.latched_mask;
  snap.dmx_online = sample->dmx_online;
  snap.pumping = sample->pumping;

  snapshot_changed = ui_service_set_machine_snapshot(&snap);
  if(snapshot_changed)
  {
    sl_ui_request_redraw();
  }
}

static void task_ui_run_slate_once(void)
{
  uint8_t tick_i;

  for(tick_i = 0U; tick_i < (uint8_t)TICKS_INTERVAL; tick_i++)
  {
    sl_ui_tick_up();
  }
  sl_ui_run_once();
}

/**
 * @brief   Initialize UI task configuration.
 *
 * @param[in] cfg UI task configuration pointer.
 *                When NULL, clear static configuration for recovery.
 */
void app_task_ui_init(const app_task_ui_cfg_t *cfg)
{
  if(cfg == 0)
  {
    memset(&s_task_ui_cfg, 0, sizeof(s_task_ui_cfg));
    return;
  }

  s_task_ui_cfg = *cfg;
}

/**
 * @brief   UI任务主体
 *
 * @param[in] pvParameters 未使用（标准 FreeRTOS 接口）
 *
 * 主循环逻辑：
 *   1. 执行一次初始化
 *   2. 处理按键事件
 *   3. 读取系统状态
 *   4. 更新UI快照
 *   5. 运行SlateUI
 *   6. 更新菜单活动状态
 *   7. 设置心跳标志
 */
void ui_task(void *pvParameters)
{
  (void)pvParameters;
  task_ui_setup_once();

  for(;;)
  {
    const char *page_name;      /**< 当前页面名称 */
    ui_runtime_sample_t sample;  /**< UI运行态采样 */

    /** 处理按键事件 */
    button_ticks();

    task_ui_collect_runtime(&sample);
    task_ui_update_snapshot_and_redraw(&sample);

    /** 运行SlateUI */
    task_ui_run_slate_once();

    /** 更新菜单活动状态 */
    page_name = sl_ui_current_page();
    task_ui_update_menu_active_flag(page_name);

    /** 设置心跳标志，通知其他任务UI任务正常运行 */
    task_ui_heartbeat_delay();
  }
}
