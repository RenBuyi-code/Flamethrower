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
 *     - app_core：使用核心功能进行状态转换
 *     - dmx_strategy：解析DMX命令生成执行意向
 *     - safety_guard：评估安全状态
 *     - task_safety：接收安全故障信息
 */

#include "task_control.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/dmx_strategy.h"
#include "../../domain/fault_manager.h"
#include "../../domain/safety_guard.h"
#include <string.h>

/** @brief 控制任务全局配置（静态单例） */
static app_task_control_cfg_t s_task_control_cfg;

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
  app_core_t *app;

  app = s_task_control_cfg.app;
  if(app == 0)
  {
    return;
  }

  if(user_mode)
  {
    fault_manager_try_clear(&app->faults, FAULT_E4_LOCKED_MODE, true);
    (void)app_core_switch_state(app, MACHINE_READY, 0x2002U, (uint32_t)now);
  }
  else
  {
    fault_manager_set(&app->faults, FAULT_E4_LOCKED_MODE);
    (void)app_core_switch_state(app, MACHINE_LOCKED, 0x2005U, (uint32_t)now);
  }

  app_task_set_fault_bits(s_task_control_cfg.event_group, app->faults.latched_mask);
  app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
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
  app_core_t *app;          /**< 应用核心实例 */

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
      fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      APP_LOGW("pressure sensor fault in selftest: raw=%u", (unsigned)pressure_raw);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2009U, (uint32_t)now);
      app_task_set_fault_bits(s_task_control_cfg.event_group, app->faults.latched_mask);
      app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
      return;
    }

    /** 检查倾斜保护 */
    if(app->params.tilt_protect_enable && tilt_fault)
    {
      fault_manager_set(&app->faults, FAULT_E2_TILT);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2006U, (uint32_t)now);
      app_task_set_fault_bits(s_task_control_cfg.event_group, app->faults.latched_mask);
      app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
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
      fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2007U, (uint32_t)now);
      app_task_set_fault_bits(s_task_control_cfg.event_group, app->faults.latched_mask);
      app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
      return;
    }

    /** 继续尝试建立压力 */
    cmd.type = ACT_CMD_PUMP_ONLY;
    cmd.user_mode = user_mode;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
    vTaskDelay(pdMS_TO_TICKS(20));
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
  bool key_menu;                 /**< 菜单键状态 */
  bool key_down;                 /**< 下键状态 */
  bool key_up;                   /**< 上键状态 */
  bool key_enter;                /**< 确认键状态 */
  uint16_t pressure_raw;         /**< 压力传感器原始值 */
  uint8_t pressure_pct;           /**< 压力百分比值 */
  test_action_t test_action;     /**< 测试操作类型 */
  test_action_t last_test_action; /**< 上一次测试操作类型 */
  edmx_frame_t frame;            /**< DMX帧数据 */
  bool pressure_ready_for_fire;  /**< 压力是否达到点火要求 */
  bool pressure_refill_active;   /**< 压力补压是否激活 */
  app_core_t *app;               /**< 应用核心实例 */
  (void)pvParameters;

  last_test_action = (test_action_t)0xFF;
  pressure_refill_active = true;

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

    /** 更新DMX在线状态 */
    if(edmx_rx_is_online(s_task_control_cfg.dmx_rx, (uint32_t)now))
    {
      (void)xEventGroupSetBits(s_task_control_cfg.event_group, EVT_DMX_ONLINE_BIT);
    }
    else
    {
      (void)xEventGroupClearBits(s_task_control_cfg.event_group, EVT_DMX_ONLINE_BIT);
    }

    bits = xEventGroupGetBits(s_task_control_cfg.event_group);
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);

    /** 处理DMX数据，生成执行意向 */
    ok = edmx_rx_copy_latest(s_task_control_cfg.dmx_rx, &frame);
    if(ok)
    {
      ok = dmx_strategy_build_intent(app->params.dmx_mode, frame.channels, app->params.dmx_address, &intent);
    }
    if(ok == false)
    {
      intent.request_fire = false;
      intent.request_relief = false;
      intent.fire_duration_ms = 0U;
      if((bits & EVT_DMX_ONLINE_BIT) != 0U)
      {
        APP_LOGW("dmx invalid -> safe_off");
      }
    }

    /** 准备安全评估输入 */
    in.latched_fault_mask = app_task_read_fault_mask_from_events(bits);
    in.dmx_online = ((bits & EVT_DMX_ONLINE_BIT) != 0U) ? 1 : 0;
    in.relief_requested = intent.request_relief ? 1 : 0;
    in.fire_requested = intent.request_fire ? 1 : 0;
    in.in_user_mode = user_mode ? 1 : 0;
    in.tilt_fault_active = (app->params.tilt_protect_enable && app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH)) ? 1 : 0;
    in.voltage_ok = 1;
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    in.pressure_pct = pressure_pct;
    in.pressure_fire_min_pct = CFG_PRESSURE_FIRE_MIN_PCT;

    /** 检查压力传感器故障 */
    if(cfg_pressure_sensor_fault(pressure_raw))
    {
      fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      app_task_set_fault_bits(s_task_control_cfg.event_group, app->faults.latched_mask);
      APP_LOGW("pressure sensor fault in control: raw=%u", (unsigned)pressure_raw);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2205U, (uint32_t)now);
      app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
      (void)xEventGroupSetBits(s_task_control_cfg.event_group, s_task_control_cfg.hb_bit);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    /** 准备执行器命令 */
    cmd.priority = 1U;
    cmd.igniter_delay_ms = app->params.igniter_delay_ms;
    cmd.oil_lock_delay_ms = app->params.oil_lock_delay_ms;
    cmd.fire_duration_ms = intent.fire_duration_ms;
    cmd.user_mode = user_mode;

    /** 非用户模式（测试模式） */
    if(user_mode == false)
    {
      if((s_task_control_cfg.ui_menu_active != 0) && (*s_task_control_cfg.ui_menu_active == true))
      {
        cmd.type = ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
        (void)xEventGroupSetBits(s_task_control_cfg.event_group, s_task_control_cfg.hb_bit);
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      key_menu = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_MENU);
      key_down = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_DOWN);
      key_up = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_UP);
      key_enter = app->hal.input.read(app->hal.input.ctx, INPUT_KEY_ENTER);

      if((in.latched_fault_mask & FAULT_MASK_FATAL) != 0U)
      {
        (void)app_core_switch_state(app, MACHINE_FAULT, 0x2301U, (uint32_t)now);
        app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
      }
      else
      {
        /** 根据按键或DMX信号确定测试操作 */
        if(key_menu)
        {
          test_action = TEST_ACT_SAFE_OFF;
        }
        else if(key_down)
        {
          test_action = TEST_ACT_RELIEF;
        }
        else if(key_enter)
        {
          test_action = TEST_ACT_FIRE;
        }
        else if(key_up)
        {
          test_action = TEST_ACT_PUMP_ONLY;
        }
        else if(((bits & EVT_DMX_ONLINE_BIT) != 0U) && (ok == true))
        {
          if(intent.request_relief)
          {
            test_action = TEST_ACT_RELIEF;
          }
          else if(intent.request_fire)
          {
            test_action = TEST_ACT_FIRE;
          }
          else
          {
            test_action = TEST_ACT_SAFE_OFF;
          }
        }
        else
        {
          test_action = TEST_ACT_SAFE_OFF;
        }

        /** 执行测试操作 */
        if(test_action != last_test_action)
        {
          switch(test_action)
          {
            case TEST_ACT_PUMP_ONLY:
              cmd.type = ACT_CMD_PUMP_ONLY;
              APP_LOGI("test mode action=PUMP");
              break;
            case TEST_ACT_RELIEF:
              cmd.type = ACT_CMD_RELIEF;
              APP_LOGI("test mode action=RELIEF");
              break;
            case TEST_ACT_FIRE:
              cmd.type = ACT_CMD_FIRE;
              APP_LOGI("test mode action=IGNITER_TEST");
              break;
            case TEST_ACT_SAFE_OFF:
            default:
              cmd.type = ACT_CMD_SAFE_OFF;
              APP_LOGI("test mode action=SAFE_OFF");
              break;
          }

          app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, (cmd.type == ACT_CMD_SAFE_OFF) ? pdTRUE : pdFALSE);
          last_test_action = test_action;
        }
      }

      (void)xEventGroupSetBits(s_task_control_cfg.event_group, s_task_control_cfg.hb_bit);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    last_test_action = (test_action_t)0xFF;

    /** 用户模式：根据安全评估结果执行操作 */
    switch(safety_guard_eval(&in))
    {
      case SAFETY_FORCE_STOP:
        (void)app_core_switch_state(app, MACHINE_FAULT, 0x2201U, (uint32_t)now);
        app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
        break;
      case SAFETY_LOCKED:
        (void)app_core_switch_state(app, MACHINE_LOCKED, 0x2202U, (uint32_t)now);
        app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        cmd.type = ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
        break;
      case SAFETY_FORCE_RELIEF:
        (void)app_core_switch_state(app, MACHINE_RELIEF, 0x2203U, (uint32_t)now);
        app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        cmd.type = ACT_CMD_RELIEF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
        break;
      case SAFETY_ALLOW_FIRE:
      default:
        pressure_ready_for_fire = (pressure_pct >= CFG_PRESSURE_FIRE_MIN_PCT);
        if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
        {
          pressure_refill_active = false;
        }
        else if(pressure_pct <= CFG_PRESSURE_REFILL_RESUME_PCT)
        {
          pressure_refill_active = true;
        }

        if(intent.request_relief)
        {
          (void)app_core_switch_state(app, MACHINE_RELIEF, 0x2204U, (uint32_t)now);
          app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
          cmd.type = ACT_CMD_RELIEF;
          app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
        }
        else if(intent.request_fire && pressure_ready_for_fire)
        {
          cmd.type = ACT_CMD_FIRE;
          app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(app, MACHINE_FIRING, 0x2003U, (uint32_t)now);
          app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        }
        else if(intent.request_fire)
        {
          cmd.type = ACT_CMD_PUMP_ONLY;
          app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(app, MACHINE_READY, 0x2008U, (uint32_t)now);
          app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        }
        else
        {
          cmd.type = pressure_refill_active ? ACT_CMD_PUMP_ONLY : ACT_CMD_SAFE_OFF;
          app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
          (void)app_core_switch_state(app, MACHINE_READY, 0x2004U, (uint32_t)now);
          app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
        }
        break;
    }

    /** 设置心跳标志，通知其他任务控制任务正常运行 */
    (void)xEventGroupSetBits(s_task_control_cfg.event_group, s_task_control_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
