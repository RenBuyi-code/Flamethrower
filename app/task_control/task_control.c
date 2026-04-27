/**
 * @file    task_control.c
 * @brief   控制任务实现
 *
 * 控制任务模块，负责：
 *   - 处理DMX命令解析和执行
 *   - 管理系统状态转换（就绪、点火、泄压等）
 *   - 执行启动自检
 *   - 处理用户模式和测试模式
 *   - 协调安全任务和执行器任务
 *
 * 设计思路：
 *   - 定期处理DMX数据
 *   - 根据安全评估结果执行相应操作
 *   - 管理压力状态和自动补压
 *   - 与其他模块的关系：
 *     - app_fsm：使用核心功能进行状态转换
 *     - dmx_strategy：解析DMX命令生成执行意向
 *     - safety_guard：评估安全状态
 *     - task_safety：接收安全故障信息
 */

#include "task_control.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../rules/dmx_strategy.h"
#include "../rules/fault_manager.h"
#include "../rules/safety_guard.h"
#include <string.h>

/** @brief 控制任务全局配置（静态单例） */
static app_task_control_cfg_t s_task_control_cfg;

typedef struct
{
  test_action_t last_test_action;
  bool pressure_refill_active;
} task_control_runtime_t;

static void task_control_transition(app_fsm_t *app, machine_state_t state, uint16_t event_id, TickType_t now);

/**
 * @brief   设置模式状态（用户模式/锁定模式）
 *
 * @param[in] user_mode  是否为用户模式
 * @param[in] now        当前系统时间
 *
 * 操作流程：
 *   1. 根据用户模式设置或清除锁定故障
 *   2. 切换到相应的系统状态
 *   3. 更新故障和状态事件标志
 */
static void task_control_set_mode_state(bool user_mode, TickType_t now)
{
  app_fsm_t *app;

  app = s_task_control_cfg.app;
  if(app == 0)
  {
    return;
  }

  if(user_mode)
  {
    task_control_transition(app, MACHINE_READY, 0x2002U, now);
  }
  else
  {
    task_control_transition(app, MACHINE_LOCKED, 0x2005U, now);
  }
}

/**
 * @brief   执行启动自检
 *
 * 自检流程：
 *   1. 发送安全关闭命令
 *   2. 检查传感器状态（压力、倾斜开关）
 *   3. 尝试建立压力
 *   4. 根据自检结果切换到相应状态
 *
 * 自检失败情况：
 *   - 压力传感器故障
 *   - 倾斜保护触发
 *   - 压力建立超时
 */
static void task_control_run_startup_selftest(void)
{
  TickType_t start_tick;    /**< 自检开始时间 */
  TickType_t now;           /**< 当前系统时间 */
  bool user_mode;           /**< 用户模式状态 */
  bool tilt_fault;          /**< 倾斜故障状态 */
  uint16_t pressure_raw;    /**< 压力传感器原始值 */
  uint8_t pressure_pct;      /**< 压力百分比值 */
  actuator_cmd_t cmd;       /**< 执行器命令 */
  app_fsm_t *app;          /**< 应用核心实例 */

  app = s_task_control_cfg.app;
  if((app == 0) || (s_task_control_cfg.q_actuator == 0))
  {
    return;
  }

  start_tick = xTaskGetTickCount();
  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 5U;
  cmd.user_mode = false;
  cmd.igniter_delay_ms = 0U;
  cmd.oil_lock_delay_ms = 0U;
  cmd.fire_duration_ms = 0U;
  app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);

  for(;;)
  {
    now = xTaskGetTickCount();
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH);
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);

    /** 检查压力传感器故障 */
    if(cfg_pressure_sensor_fault(pressure_raw))
    {
      APP_LOGW("pressure sensor fault in selftest: raw=%u", (unsigned)pressure_raw);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2009U, now);
      return;
    }

    /** 检查倾斜保护 */
    if(app->params.tilt_protect_enable && tilt_fault)
    {
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2006U, now);
      return;
    }

    /** 压力达到目标值，自检成功 */
    if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
    {
      cmd.type = ACT_CMD_SAFE_OFF;
      cmd.user_mode = user_mode;
      app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
      task_control_set_mode_state(user_mode, now);
      return;
    }

    /** 压力建立超时，自检失败 */
    if((now - start_tick) >= pdMS_TO_TICKS(CFG_SELFTEST_PRESSURE_TIMEOUT_MS))
    {
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2007U, now);
      return;
    }

    /** 继续尝试建立压力 */
    cmd.type = ACT_CMD_PUMP_ONLY;
    cmd.user_mode = user_mode;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void task_control_heartbeat_delay(void)
{
  (void)xEventGroupSetBits(s_task_control_cfg.event_group, s_task_control_cfg.hb_bit);
  vTaskDelay(pdMS_TO_TICKS(10));
}

static void task_control_update_dmx_online(edmx_rx_t *dmx_rx, EventGroupHandle_t event_group, TickType_t now)
{
  if(edmx_rx_is_online(dmx_rx, (uint32_t)now))
  {
    (void)xEventGroupSetBits(event_group, EVT_DMX_ONLINE_BIT);
  }
  else
  {
    (void)xEventGroupClearBits(event_group, EVT_DMX_ONLINE_BIT);
  }
}

static bool task_control_build_dmx_intent(app_fsm_t *app, edmx_rx_t *dmx_rx, EventBits_t bits, dmx_intent_t *intent)
{
  edmx_frame_t frame;
  bool ok;

  ok = edmx_rx_copy_latest(dmx_rx, &frame);
  if(ok)
  {
    ok = dmx_strategy_build_intent(app->params.dmx_mode, frame.channels, app->params.dmx_address, intent);
  }

  if(ok == false)
  {
    intent->request_fire = false;
    intent->request_relief = false;
    intent->fire_duration_ms = 0U;
    if((bits & EVT_DMX_ONLINE_BIT) != 0U)
    {
      APP_LOGW("dmx invalid -> safe_off");
    }
  }

  return ok;
}

static void task_control_fill_safety_input(safety_eval_input_t *in,
                                           app_fsm_t *app,
                                           EventBits_t bits,
                                           const dmx_intent_t *intent,
                                           bool user_mode,
                                           uint8_t pressure_pct)
{
  in->latched_fault_mask = app->faults.latched_mask;
  in->dmx_online = ((bits & EVT_DMX_ONLINE_BIT) != 0U) ? 1 : 0;
  in->relief_requested = intent->request_relief ? 1 : 0;
  in->fire_requested = intent->request_fire ? 1 : 0;
  in->in_user_mode = user_mode ? 1 : 0;
  in->tilt_fault_active = (app->params.tilt_protect_enable && app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH)) ? 1 : 0;
  in->voltage_ok = 1;
  in->pressure_pct = pressure_pct;
  in->pressure_fire_min_pct = CFG_PRESSURE_FIRE_MIN_PCT;
}

static void task_control_init_actuator_cmd(actuator_cmd_t *cmd,
                                           const app_fsm_t *app,
                                           const dmx_intent_t *intent,
                                           bool user_mode)
{
  cmd->priority = 1U;
  cmd->igniter_delay_ms = app->params.igniter_delay_ms;
  cmd->oil_lock_delay_ms = app->params.oil_lock_delay_ms;
  cmd->fire_duration_ms = intent->fire_duration_ms;
  cmd->user_mode = user_mode;
}

static void task_control_transition(app_fsm_t *app, machine_state_t state, uint16_t event_id, TickType_t now)
{
  (void)app_fsm_transition(app, state, event_id, (uint32_t)now);
  app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
}

static bool task_control_handle_pressure_sensor_fault(app_fsm_t *app, uint16_t pressure_raw, TickType_t now)
{
  if(cfg_pressure_sensor_fault(pressure_raw) == false)
  {
    return false;
  }

  APP_LOGW("pressure sensor fault in control: raw=%u", (unsigned)pressure_raw);
  app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
  task_control_transition(app, MACHINE_FAULT, 0x2205U, now);
  task_control_heartbeat_delay();

  return true;
}

static test_action_t task_control_choose_test_action(bool key_menu,
                                                     bool key_down,
                                                     bool key_up,
                                                     bool key_enter,
                                                     EventBits_t bits,
                                                     bool dmx_ok,
                                                     const dmx_intent_t *intent)
{
  if(key_menu)
  {
    return TEST_ACT_SAFE_OFF;
  }
  if(key_down)
  {
    return TEST_ACT_RELIEF;
  }
  if(key_enter)
  {
    return TEST_ACT_FIRE;
  }
  if(key_up)
  {
    return TEST_ACT_PUMP_ONLY;
  }
  if(((bits & EVT_DMX_ONLINE_BIT) != 0U) && (dmx_ok == true))
  {
    if(intent->request_relief)
    {
      return TEST_ACT_RELIEF;
    }
    if(intent->request_fire)
    {
      return TEST_ACT_FIRE;
    }
  }

  return TEST_ACT_SAFE_OFF;
}

static void task_control_send_test_action(actuator_cmd_t *cmd, test_action_t test_action)
{
  switch(test_action)
  {
    case TEST_ACT_PUMP_ONLY:
      cmd->type = ACT_CMD_PUMP_ONLY;
      APP_LOGI("test mode action=PUMP");
      break;
    case TEST_ACT_RELIEF:
      cmd->type = ACT_CMD_RELIEF;
      APP_LOGI("test mode action=RELIEF");
      break;
    case TEST_ACT_FIRE:
      cmd->type = ACT_CMD_FIRE;
      APP_LOGI("test mode action=IGNITER_TEST");
      break;
    case TEST_ACT_SAFE_OFF:
    default:
      cmd->type = ACT_CMD_SAFE_OFF;
      APP_LOGI("test mode action=SAFE_OFF");
      break;
  }

  app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, (cmd->type == ACT_CMD_SAFE_OFF) ? pdTRUE : pdFALSE);
}

static void task_control_run_test_mode(app_fsm_t *app,
                                       TickType_t now,
                                       EventBits_t bits,
                                       const safety_eval_input_t *in,
                                       bool dmx_ok,
                                       const dmx_intent_t *intent,
                                       actuator_cmd_t *cmd,
                                       task_control_runtime_t *rt)
{
  bool key_menu;
  bool key_down;
  bool key_up;
  bool key_enter;
  test_action_t test_action;

  if((s_task_control_cfg.ui_menu_active != 0) && (*s_task_control_cfg.ui_menu_active == true))
  {
    cmd->type = ACT_CMD_SAFE_OFF;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdTRUE);
    task_control_heartbeat_delay();
    return;
  }

  key_menu = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_MENU);
  key_down = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_DOWN);
  key_up = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_UP);
  key_enter = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_ENTER);

  if((in->latched_fault_mask & FAULT_MASK_FATAL) != 0U)
  {
    task_control_transition(app, MACHINE_FAULT, 0x2301U, now);
    cmd->type = ACT_CMD_SAFE_OFF;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdTRUE);
    task_control_heartbeat_delay();
    return;
  }

  test_action = task_control_choose_test_action(key_menu, key_down, key_up, key_enter, bits, dmx_ok, intent);
  if(test_action != rt->last_test_action)
  {
    task_control_send_test_action(cmd, test_action);
    rt->last_test_action = test_action;
  }

  task_control_heartbeat_delay();
}

static void task_control_run_user_mode(app_fsm_t *app,
                                       TickType_t now,
                                       uint8_t pressure_pct,
                                       const dmx_intent_t *intent,
                                       const safety_eval_input_t *in,
                                       actuator_cmd_t *cmd,
                                       task_control_runtime_t *rt)
{
  bool pressure_ready_for_fire;

  rt->last_test_action = (test_action_t)0xFF;

  switch(safety_guard_eval(in))
  {
    case SAFETY_FORCE_STOP:
      task_control_transition(app, MACHINE_FAULT, 0x2201U, now);
      cmd->type = ACT_CMD_SAFE_OFF;
      app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdTRUE);
      break;
    case SAFETY_LOCKED:
      task_control_transition(app, MACHINE_LOCKED, 0x2202U, now);
      cmd->type = ACT_CMD_SAFE_OFF;
      app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdTRUE);
      break;
    case SAFETY_FORCE_RELIEF:
      task_control_transition(app, MACHINE_RELIEF, 0x2203U, now);
      cmd->type = ACT_CMD_RELIEF;
      app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
      break;
    case SAFETY_ALLOW_FIRE:
    default:
      pressure_ready_for_fire = (pressure_pct >= CFG_PRESSURE_FIRE_MIN_PCT);
      if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
      {
        rt->pressure_refill_active = false;
      }
      else if(pressure_pct <= CFG_PRESSURE_REFILL_RESUME_PCT)
      {
        rt->pressure_refill_active = true;
      }

      if(intent->request_relief)
      {
        task_control_transition(app, MACHINE_RELIEF, 0x2204U, now);
        cmd->type = ACT_CMD_RELIEF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
      }
      else if(intent->request_fire && pressure_ready_for_fire)
      {
        cmd->type = ACT_CMD_FIRE;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
        task_control_transition(app, MACHINE_FIRING, 0x2003U, now);
      }
      else if(intent->request_fire)
      {
        cmd->type = ACT_CMD_PUMP_ONLY;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
        task_control_transition(app, MACHINE_READY, 0x2008U, now);
      }
      else
      {
        cmd->type = rt->pressure_refill_active ? ACT_CMD_PUMP_ONLY : ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
        task_control_transition(app, MACHINE_READY, 0x2004U, now);
      }
      break;
  }
}

/**
 * @brief   初始化控制任务配置
 *
 * @param[in] cfg  控制任务配置结构体指针
 *
 * 当 cfg == NULL 时，清除配置（用于异常恢复）
 */
void app_task_control_init(const app_task_control_cfg_t *cfg)
{
  if(cfg == 0)
  {
    memset(&s_task_control_cfg, 0, sizeof(s_task_control_cfg));
    return;
  }

  s_task_control_cfg = *cfg;
}

/**
 * @brief   控制任务主体
 *
 * @param[in] pvParameters 未使用（标准 FreeRTOS 接口）
 *
 * 主循环逻辑：
 *   1. 执行启动自检
 *   2. 定期处理DMX数据
 *   3. 读取用户输入和传感器数据
 *   4. 评估安全状态
 *   5. 根据评估结果执行相应操作
 *   6. 管理系统状态转换
 *
 * 运行模式：
 *   - 测试模式：通过按键控制执行器
 *   - 用户模式：通过DMX控制执行器
 */
void control_task(void *pvParameters)
{
  TickType_t now;                /**< 当前系统时间 */
  EventBits_t bits;              /**< 事件标志位 */
  actuator_cmd_t cmd;            /**< 执行器命令 */
  dmx_intent_t intent;           /**< DMX执行意向 */
  safety_eval_input_t in;        /**< 安全评估输入 */
  bool ok;                       /**< 操作结果 */
  bool user_mode;                /**< 用户模式状态 */
  uint16_t pressure_raw;         /**< 压力传感器原始值 */
  uint8_t pressure_pct;           /**< 压力百分比值 */
  task_control_runtime_t rt;     /**< 跨循环运行状态 */
  app_fsm_t *app;               /**< 应用核心实例 */
  (void)pvParameters;

  rt.last_test_action = (test_action_t)0xFF;
  rt.pressure_refill_active = true;

  vTaskDelay(pdMS_TO_TICKS(50));
  task_control_run_startup_selftest();

  for(;;)
  {
    app = s_task_control_cfg.app;
    if((app == 0) || (s_task_control_cfg.dmx_rx == 0) || (s_task_control_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    now = xTaskGetTickCount();
    edmx_rx_process(s_task_control_cfg.dmx_rx, (uint32_t)now);
    task_control_update_dmx_online(s_task_control_cfg.dmx_rx, s_task_control_cfg.event_group, now);

    bits = xEventGroupGetBits(s_task_control_cfg.event_group);
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);

    /** 处理DMX数据，生成执行意向 */
    ok = task_control_build_dmx_intent(app, s_task_control_cfg.dmx_rx, bits, &intent);

    /** 准备安全评估输入 */
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    task_control_fill_safety_input(&in, app, bits, &intent, user_mode, pressure_pct);

    /** 检查压力传感器故障 */
    if(task_control_handle_pressure_sensor_fault(app, pressure_raw, now))
    {
      continue;
    }

    /** 准备执行器命令 */
    task_control_init_actuator_cmd(&cmd, app, &intent, user_mode);

    /** 非用户模式（测试模式） */
    if(user_mode == false)
    {
      task_control_run_test_mode(app, now, bits, &in, ok, &intent, &cmd, &rt);
      continue;
    }

    /** 用户模式：根据安全评估结果执行操作 */
    task_control_run_user_mode(app, now, pressure_pct, &intent, &in, &cmd, &rt);

    /** 设置心跳标志，通知其他任务控制任务正常运行 */
    task_control_heartbeat_delay();
  }
}
