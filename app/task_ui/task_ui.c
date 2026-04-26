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
 *     - app_core：获取系统状态和参数
 *     - task_control：通过menu_active标志通知控制任务
 *     - SlateUI：使用其页面管理和显示功能
 */

#include "task_ui.h"
#include "../ui_services.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/dmx_strategy.h"
#include "../../middleware/MultiButton/multi_button.h"
//#define SL_PAGE_TRANSITION_MS 0
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

/** @brief UI任务全局配置（静态单例） */
static app_task_ui_cfg_t s_task_ui_cfg;
/** @brief UI按钮上下文 */
static ui_button_ctx_t s_ui_btn;
/** @brief DMX地址影子值（用于设置页面） */
static int16_t s_shadow_dmx_addr;
/** @brief DMX模式影子值（用于设置页面） */
static int16_t s_shadow_dmx_mode;
/** @brief 点火延迟影子值（用于设置页面） */
static int16_t s_shadow_ign_delay;
/** @brief 锁定延迟影子值（用于设置页面） */
static int16_t s_shadow_lock_delay;
/** @brief 倾斜保护启用影子值（用于设置页面） */
static int16_t s_shadow_tilt_enable;
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

/**
 * @brief   提交参数修改
 *
 * 操作流程：
 *   1. 对参数进行校验和修正
 *   2. 调用提交回调函数
 */
static void task_ui_commit_params(void)
{
  if((s_task_ui_cfg.app == 0) || (s_task_ui_cfg.commit_params == 0))
  {
    return;
  }

  cfg_sanitize_params(&s_task_ui_cfg.app->params);
  s_task_ui_cfg.commit_params();
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
  s_task_ui_cfg.app->params.dmx_address = (uint16_t)value;
  APP_LOGI("dmx addr=%u", (unsigned)s_task_ui_cfg.app->params.dmx_address);
  task_ui_commit_params();
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
  s_task_ui_cfg.app->params.dmx_mode = (value == 1) ? DMX_MODE_6CH : DMX_MODE_2CH;
  APP_LOGI("dmx mode=%u", (unsigned)s_task_ui_cfg.app->params.dmx_mode);
  task_ui_commit_params();
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
  s_task_ui_cfg.app->params.igniter_delay_ms = (uint16_t)value;
  APP_LOGI("ign delay=%u", (unsigned)s_task_ui_cfg.app->params.igniter_delay_ms);
  task_ui_commit_params();
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
  s_task_ui_cfg.app->params.oil_lock_delay_ms = (uint16_t)value;
  APP_LOGI("lock delay=%u", (unsigned)s_task_ui_cfg.app->params.oil_lock_delay_ms);
  task_ui_commit_params();
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
  s_task_ui_cfg.app->params.tilt_protect_enable = (value != 0) ? true : false;
  APP_LOGI("tilt protect=%u", (unsigned)s_task_ui_cfg.app->params.tilt_protect_enable);
  task_ui_commit_params();
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
  s_task_ui_cfg.app->params.language = (uint8_t)value;
  APP_LOGI("language=%u", (unsigned)s_task_ui_cfg.app->params.language);
  task_ui_commit_params();
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

/**
 * @brief   UI任务初始化（仅执行一次）
 *
 * 操作流程：
 *   1. 初始化影子参数值
 *   2. 设置页面引用
 *   3. 绑定设置页面回调函数
 *   4. 初始化SlateUI和端口
 *   5. 初始化按键
 *   6. 注册UI页面
 *   7. 初始化SlateUI
 */
static void task_ui_setup_once(void)
{
  ui_setting_handlers_t handlers;

  if((s_task_ui_cfg.app == 0) || s_ui_btn.initialized)
  {
    return;
  }

  /** 初始化影子参数值 */
  s_shadow_dmx_addr = (int16_t)s_task_ui_cfg.app->params.dmx_address;
  s_shadow_dmx_mode = (int16_t)(s_task_ui_cfg.app->params.dmx_mode == DMX_MODE_6CH ? 1 : 0);
  s_shadow_ign_delay = (int16_t)s_task_ui_cfg.app->params.igniter_delay_ms;
  s_shadow_lock_delay = (int16_t)s_task_ui_cfg.app->params.oil_lock_delay_ms;
  s_shadow_tilt_enable = s_task_ui_cfg.app->params.tilt_protect_enable ? 1 : 0;

  /** 设置页面引用 */
  ui_setting_page_set_dmx_refs(&s_shadow_dmx_addr, &s_shadow_dmx_mode);
  ui_setting_page_set_pressure_refs(&s_shadow_ign_delay, &s_shadow_lock_delay);
  ui_safety_page_set_tilt_ref(&s_shadow_tilt_enable);

  /** 绑定设置页面回调函数 */
  handlers.save_dmx_addr = task_ui_save_dmx_addr;
  handlers.save_dmx_mode = task_ui_save_dmx_mode;
  handlers.save_ign_delay = task_ui_save_ign_delay;
  handlers.save_lock_delay = task_ui_save_lock_delay;
  handlers.save_tilt_enable = task_ui_save_tilt_enable;
  handlers.save_language = task_ui_save_language;
  ui_service_bind_setting_handlers(&handlers);

  /** 初始化SlateUI和端口 */
  sl_port_init();
  sl_port_input_init();

  /** 读取并记录初始按键状态 */
  {
    uint8_t m = task_ui_button_level_read(UI_BTN_ID_MENU);
    uint8_t d = task_ui_button_level_read(UI_BTN_ID_DOWN);
    uint8_t u = task_ui_button_level_read(UI_BTN_ID_UP);
    uint8_t e = task_ui_button_level_read(UI_BTN_ID_ENTER);
    APP_LOGI("btn init state M=%u D=%u U=%u E=%u", (unsigned)m, (unsigned)d, (unsigned)u, (unsigned)e);
  }

  /** 初始化按键 */
  button_init(&s_ui_btn.key_menu, task_ui_button_level_read, 1U, UI_BTN_ID_MENU);
  button_init(&s_ui_btn.key_down, task_ui_button_level_read, 1U, UI_BTN_ID_DOWN);
  button_init(&s_ui_btn.key_up, task_ui_button_level_read, 1U, UI_BTN_ID_UP);
  button_init(&s_ui_btn.key_enter, task_ui_button_level_read, 1U, UI_BTN_ID_ENTER);

  /** 绑定按键回调函数 */
  button_attach(&s_ui_btn.key_menu, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_down, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);
  button_attach(&s_ui_btn.key_up, BTN_LONG_PRESS_HOLD, task_ui_btn_repeat_cb, 0);
  button_attach(&s_ui_btn.key_enter, BTN_PRESS_DOWN, task_ui_btn_click_cb, 0);

  /** 启动按键检测 */
  (void)button_start(&s_ui_btn.key_menu);
  (void)button_start(&s_ui_btn.key_down);
  (void)button_start(&s_ui_btn.key_up);
  (void)button_start(&s_ui_btn.key_enter);

  /** 初始化显示和页面 */
  sl_lang_set((int)s_task_ui_cfg.app->params.language);
  sl_disp_init();
  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  sl_disp_flush();
  task_ui_register_pages_once();
  sl_ui_init("splash");

  s_ui_btn.initialized = true;
  APP_LOGI("ui setup done");
}

/**
 * @brief   初始化UI任务配置
 *
 * @param[in] cfg  UI任务配置结构体指针
 *
 * 当 cfg == NULL 时，清除配置（用于异常恢复）
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
    uint8_t tick_i;            /**< 循环计数器 */
    const char *page_name;      /**< 当前页面名称 */
    EventBits_t bits;           /**< 事件标志位 */
    uint16_t pressure_raw;      /**< 压力传感器原始值 */
    uint8_t pressure_pct;       /**< 压力百分比值 */
    bool dmx_online;            /**< DMX在线状态 */
    machine_state_t st;         /**< 机器状态 */
    ui_machine_snapshot_t snap;  /**< UI机器状态快照 */
    bool snapshot_changed;       /**< 快照是否变化 */
    actuator_status_t act_st;    /**< 执行器状态 */
    BaseType_t act_ok;           /**< 是否获取到执行器状态 */
    bool pumping;                /**< 是否正在泵油 */

    /** 处理按键事件 */
    button_ticks();

    /** 读取系统状态 */
    bits = xEventGroupGetBits(s_task_ui_cfg.event_group);
    pressure_raw = s_task_ui_cfg.app->hal.adc.read_raw(s_task_ui_cfg.app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    dmx_online = ((bits & s_task_ui_cfg.dmx_online_bit) != 0U);
    st = s_task_ui_cfg.app->machine.current;

    /** 读取执行器状态 */
    act_ok = xQueuePeek(s_task_ui_cfg.q_actuator_status, &act_st, 0);
    pumping = ((act_ok == pdTRUE) && act_st.out.oil_pump_on && (act_st.fire_active == false) && (act_st.relief_active == false));

    /** 更新UI快照 */
    snap.state = st;
    snap.pressure_pct = pressure_pct;
    snap.dmx_addr = s_task_ui_cfg.app->params.dmx_address;
    snap.fault_mask = s_task_ui_cfg.app->faults.latched_mask;
    snap.dmx_online = dmx_online;
    snap.pumping = pumping;
    snapshot_changed = ui_service_set_machine_snapshot(&snap);
    if(snapshot_changed)
    {
      sl_ui_request_redraw();
    }

    /** 运行SlateUI */
    for(tick_i = 0U; tick_i < (uint8_t)TICKS_INTERVAL; tick_i++)
    {
      sl_ui_tick_up();
    }
    sl_ui_run_once();

    /** 更新菜单活动状态 */
    page_name = sl_ui_current_page();
    if(s_task_ui_cfg.menu_active != 0)
    {
      *s_task_ui_cfg.menu_active = ((page_name != 0) &&
                                    (strcmp(page_name, "idle") != 0) &&
                                    (strcmp(page_name, "checking") != 0) &&
                                    (strcmp(page_name, "splash") != 0));
    }

    /** 设置心跳标志，通知其他任务UI任务正常运行 */
    (void)xEventGroupSetBits(s_task_ui_cfg.event_group, s_task_ui_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(TICKS_INTERVAL));
  }
}
