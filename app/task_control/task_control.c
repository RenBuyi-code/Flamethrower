/**
 * @file    task_control.c
 * @brief   核心控制任务 — 状态机中枢
 *
 * ## 职责
 *   控制任务是整个系统的决策中心。每个周期：
 *   1. 处理 DMX 输入 → 生成执行意向（fire / relief / idle）
 *   2. 读取传感器（压力、安全锁、倾斜开关）
 *   3. 调用安全防护模块评估 → 得到安全动作
 *   4. 根据安全动作 + DMX 意向，切换状态机并发送执行器命令
 *
 * ## 启动流程
 *   上电复位 → MACHINE_BOOT(0) → task_control_run_startup_selftest()
 *   → 循环加压直到压力达标或超时 → MACHINE_READY(2) / MACHINE_FAULT(5)
 *
 * ## 状态机总览
 *   0 BOOT      → 上电初始
 *   1 SELFTEST  → 自检（建压测试）
 *   2 READY     → 就绪，等待 DMX/按键指令
 *   3 FIRING    → 喷射中
 *   4 RELIEF    → 泄压中
 *   5 FAULT     → 故障（E1~E5）
 *   6 LOCKED    → 测试模式锁定
 *
 * ## 用户模式 vs 测试模式
 *   - 安全锁闭合（USER MODE）→ task_control_run_user_mode()
 *     DMX 控台控制，锁油阀可工作，可喷火
 *   - 安全锁断开（TEST MODE）→ task_control_run_test_mode()
 *     按键手动测试，锁油阀禁用，不喷燃料
 *
 * ## 泄压优先原则
 *   无论哪种模式，只要 DMX 解析出泄压请求就立即执行。
 *   此设计确保控台操作员可以随时紧急卸掉管道压力。
 */

#include "task_control.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../rules/dmx_strategy.h"
#include "../rules/fault_manager.h"
#include "../rules/safety_guard.h"
#include <string.h>

static app_task_control_cfg_t s_task_control_cfg;

typedef struct
{
  test_action_t last_test_action;
  bool pressure_refill_active;
} task_control_runtime_t;

static void task_control_transition(app_fsm_t *app, machine_state_t state,
                                    uint16_t event_id, TickType_t now);

/**
 * @brief   切换用户模式 / 锁定模式
 *
 * @param[in] user_mode  true=用户模式, false=测试模式
 * @param[in] now        当前系统 tick
 *
 * 安全锁开关决定机器处于用户模式还是测试模式：
 * - USER MODE：正常使用，可喷火
 * - TEST MODE：锁定机器，只能做无燃料测试
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
 * @brief   开机自检
 *
 * ## 自检流程
 *   1. 关闭所有执行器
 *   2. 循环检测压力传感器、倾斜开关
 *   3. 油泵运转建压，等待压力达到 100%
 *   4. 超时（CFG_SELFTEST_PRESSURE_TIMEOUT_MS）→ 故障
 *
 * ## 自检失败条件
 *   - 压力传感器原始值低于 546 → E1 加压故障
 *   - 倾斜开关触发（且倾斜保护开启）→ E2 倾斜故障
 *   - 超时未达到目标压力 → 建压失败
 *
 * ## 注意
 *   自检是同步阻塞的：整个任务在此期间不做其他事。
 *   这简化了启动逻辑，因为自检时没有外部输入需要处理。
 */
static void task_control_run_startup_selftest(void)
{
  TickType_t start_tick;
  TickType_t now;
  bool user_mode;
  bool tilt_fault;
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  actuator_cmd_t cmd;
  app_fsm_t *app;

  app = s_task_control_cfg.app;
  if((app == 0) || (s_task_control_cfg.q_actuator == 0))
  {
    return;
  }

  start_tick = xTaskGetTickCount();
  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 5U;
  cmd.user_mode = false;
  cmd.igniter_delay_sec = 0U;
  cmd.oil_lock_delay_sec = 0U;
  cmd.fire_duration_ms = 0U;
  app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);

  for(;;)
  {
    now = xTaskGetTickCount();
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH);
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);

    /* 压力传感器断线/损坏检测 */
    if(cfg_pressure_sensor_fault(pressure_raw))
    {
      APP_LOGW("pressure sensor fault in selftest: raw=%u",
               (unsigned)pressure_raw);
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2009U, now);
      return;
    }

    /* 倾斜保护触发检测 */
    if(app->params.tilt_protect_enable && tilt_fault)
    {
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2006U, now);
      return;
    }

    /* 压力达标 → 自检成功 */
    if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
    {
      cmd.type = ACT_CMD_SAFE_OFF;
      cmd.user_mode = user_mode;
      app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdTRUE);
      task_control_set_mode_state(user_mode, now);
      return;
    }

    /* 超时 → 建压失败 */
    if((now - start_tick) >=
       pdMS_TO_TICKS(CFG_SELFTEST_PRESSURE_TIMEOUT_MS))
    {
      app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
      task_control_transition(app, MACHINE_FAULT, 0x2007U, now);
      return;
    }

    /* 继续建压 */
    cmd.type = ACT_CMD_PUMP_ONLY;
    cmd.user_mode = user_mode;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, &cmd, pdFALSE);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void task_control_heartbeat_delay(void)
{
  (void)xEventGroupSetBits(s_task_control_cfg.event_group,
                           s_task_control_cfg.hb_bit);
  vTaskDelay(pdMS_TO_TICKS(10));
}

/**
 * @brief   更新 DMX 在线状态标志位
 *
 * 基于 easyDMX 的在线检测机制（距上次有效帧的时间间隔）
 * 更新 event group 中的 EVT_DMX_ONLINE_BIT。
 */
static void task_control_update_dmx_online(edmx_rx_t *dmx_rx,
    EventGroupHandle_t event_group, TickType_t now)
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

/**
 * @brief   DMX 数据变更日志（值变化时才输出，避免刷屏）
 *
 * 使用静态变量记住上次值，仅在发生变化时打印一行 RTT 日志。
 * 日志格式示例：
 *   dmx intent ok=1 addr=1 mode=6 ch=0,0,255,100,0,100 fire=1 relief=0 dur=1000 seq=42
 */
static void task_control_dmx_business_log(app_fsm_t *app,
    edmx_rx_t *dmx_rx,
    EventBits_t bits,
    bool intent_ok,
    const dmx_intent_t *intent)
{
  static bool s_seen_once;
  static bool s_last_online;
  static bool s_last_ok;
  static bool s_last_fire;
  static bool s_last_relief;
  static uint16_t s_last_duration_ms;
  static uint8_t s_last_ch1;
  static uint8_t s_last_ch2;
  static uint8_t s_last_ch3;
  static uint8_t s_last_ch4;
  static uint8_t s_last_ch5;
  static uint8_t s_last_ch6;
  edmx_frame_t frame;
  uint16_t base;
  uint8_t ch1;
  uint8_t ch2;
  uint8_t ch3;
  uint8_t ch4;
  uint8_t ch5;
  uint8_t ch6;

  if((app == 0) || (dmx_rx == 0) || (intent == 0))
  {
    return;
  }

  base = (app->params.dmx_address > 0U)
         ? (uint16_t)(app->params.dmx_address - 1U) : 0U;
  ch1 = 0U;
  ch2 = 0U;
  ch3 = 0U;
  ch4 = 0U;
  ch5 = 0U;
  ch6 = 0U;
  memset(&frame, 0, sizeof(frame));

  if((base < EDMX_UNIVERSE_SIZE) && edmx_rx_copy_latest(dmx_rx, &frame))
  {
    ch1 = frame.channels[base];
    if((base + 1U) < EDMX_UNIVERSE_SIZE)
    { ch2 = frame.channels[(uint16_t)(base + 1U)]; }
    if((base + 2U) < EDMX_UNIVERSE_SIZE)
    { ch3 = frame.channels[(uint16_t)(base + 2U)]; }
    if((base + 3U) < EDMX_UNIVERSE_SIZE)
    { ch4 = frame.channels[(uint16_t)(base + 3U)]; }
    if((base + 4U) < EDMX_UNIVERSE_SIZE)
    { ch5 = frame.channels[(uint16_t)(base + 4U)]; }
    if((base + 5U) < EDMX_UNIVERSE_SIZE)
    { ch6 = frame.channels[(uint16_t)(base + 5U)]; }
  }

  /* DMX 在线状态变化 */
  if((s_seen_once == false) ||
     (s_last_online != ((bits & EVT_DMX_ONLINE_BIT) != 0U)))
  {
    APP_LOGI("dmx %s",
             ((bits & EVT_DMX_ONLINE_BIT) != 0U) ? "online" : "offline");
  }

  /* DMX 数据变化 */
  if((s_seen_once == false)
     || (s_last_ok != intent_ok)
     || (s_last_fire != intent->request_fire)
     || (s_last_relief != intent->request_relief)
     || (s_last_duration_ms != intent->fire_duration_ms)
     || (s_last_ch1 != ch1)
     || (s_last_ch2 != ch2)
     || (s_last_ch3 != ch3)
     || (s_last_ch4 != ch4)
     || (s_last_ch5 != ch5)
     || (s_last_ch6 != ch6))
  {
    APP_LOGI("dmx intent ok=%u addr=%u mode=%u"
             " ch=%u,%u,%u,%u,%u,%u"
             " fire=%u relief=%u dur=%u seq=%lu",
             (unsigned)(intent_ok ? 1U : 0U),
             (unsigned)app->params.dmx_address,
             (unsigned)app->params.dmx_mode,
             (unsigned)ch1, (unsigned)ch2, (unsigned)ch3,
             (unsigned)ch4, (unsigned)ch5, (unsigned)ch6,
             (unsigned)(intent->request_fire ? 1U : 0U),
             (unsigned)(intent->request_relief ? 1U : 0U),
             (unsigned)intent->fire_duration_ms,
             (unsigned long)frame.sequence);
  }

  s_seen_once = true;
  s_last_online = ((bits & EVT_DMX_ONLINE_BIT) != 0U);
  s_last_ok = intent_ok;
  s_last_fire = intent->request_fire;
  s_last_relief = intent->request_relief;
  s_last_duration_ms = intent->fire_duration_ms;
  s_last_ch1 = ch1;
  s_last_ch2 = ch2;
  s_last_ch3 = ch3;
  s_last_ch4 = ch4;
  s_last_ch5 = ch5;
  s_last_ch6 = ch6;
}

/**
 * @brief   将 DMX 帧解析为执行意向
 *
 * 委托给 dmx_strategy_build_intent() 执行具体协议解析。
 * 如果解析失败，返回零填充的 intent（fire=false, relief=false）。
 */
static bool task_control_build_dmx_intent(app_fsm_t *app,
    edmx_rx_t *dmx_rx,
    EventBits_t bits,
    dmx_intent_t *intent)
{
  edmx_frame_t frame;
  bool ok;

  ok = edmx_rx_copy_latest(dmx_rx, &frame);
  if(ok)
  {
    ok = dmx_strategy_build_intent(app->params.dmx_mode,
                                   frame.channels,
                                   app->params.dmx_address,
                                   intent);
  }

  if(ok == false)
  {
    intent->request_fire = false;
    intent->request_relief = false;
    intent->fire_duration_ms = 0U;
  }

  return ok;
}

/**
 * @brief   准备安全评估输入数据
 *
 * 收集当前周期所有传感器和状态信息，打包成
 * safety_eval_input_t 供 safety_guard_eval() 评估。
 */
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
  in->tilt_fault_active =
      (app->params.tilt_protect_enable &&
       app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH)) ? 1 : 0;
  in->voltage_ok = 1;      /* 电压保护默认关闭 */
  in->pressure_pct = pressure_pct;
  in->pressure_fire_min_pct = CFG_PRESSURE_FIRE_MIN_PCT;
}

/**
 * @brief   初始化执行器命令结构
 *
 * 从系统参数和 DMX 意向中提取字段，组装 actuator_cmd_t。
 * 点火延时和锁阀延时来自系统参数（秒），
 * 喷射时长来自 DMX 解析结果（毫秒）。
 */
static void task_control_init_actuator_cmd(actuator_cmd_t *cmd,
    const app_fsm_t *app,
    const dmx_intent_t *intent,
    bool user_mode)
{
  cmd->priority = 1U;
  cmd->igniter_delay_sec = app->params.igniter_delay_sec;
  cmd->oil_lock_delay_sec = app->params.oil_lock_delay_sec;
  cmd->fire_duration_ms = intent->fire_duration_ms;
  cmd->user_mode = user_mode;
}

/**
 * @brief   安全的原子化状态切换
 *
 * 封装了 app_fsm_transition() 和事件标志更新两个步骤，
 * 确保状态切换和通知原子发生。
 */
static void task_control_transition(app_fsm_t *app,
    machine_state_t state, uint16_t event_id, TickType_t now)
{
  (void)app_fsm_transition(app, state, event_id, (uint32_t)now);
  app_task_set_state_bits(s_task_control_cfg.event_group, app->machine.current);
}

/**
 * @brief   压力传感器故障检测
 *
 * 在主循环每个周期调用。如果压力传感器读数异常
 * （原始值 < CFG_PRESSURE_ADC_MIN_RAW），
 * 立即安全停机并切换到 FAULT 状态。
 *
 * @return    true=检测到故障并已处理，false=传感器正常
 */
static bool task_control_handle_pressure_sensor_fault(
    app_fsm_t *app, uint16_t pressure_raw, TickType_t now)
{
  if(cfg_pressure_sensor_fault(pressure_raw) == false)
  {
    return false;
  }

  APP_LOGW("pressure sensor fault in control: raw=%u",
           (unsigned)pressure_raw);
  app_task_send_safe_off_high_prio(s_task_control_cfg.q_actuator);
  task_control_transition(app, MACHINE_FAULT, 0x2205U, now);
  task_control_heartbeat_delay();

  return true;
}

/**
 * @brief   根据用户输入和 DMX 数据选择测试动作
 *
 * 测试模式下的动作选择逻辑：
 *   - 按键优先级高于 DMX
 *   - MENU → 安全关闭
 *   - DOWN → 泄压
 *   - ENTER → 点火测试
 *   - UP → 仅建压
 *   - DMX 数据 → 跟随 DMX 意向
 */
static test_action_t task_control_choose_test_action(
    bool key_menu, bool key_down, bool key_up, bool key_enter,
    EventBits_t bits, bool dmx_ok, const dmx_intent_t *intent)
{
  if(key_menu)   { return TEST_ACT_SAFE_OFF; }
  if(key_down)   { return TEST_ACT_RELIEF; }
  if(key_enter)  { return TEST_ACT_FIRE; }
  if(key_up)     { return TEST_ACT_PUMP_ONLY; }
  if(((bits & EVT_DMX_ONLINE_BIT) != 0U) && (dmx_ok == true))
  {
    if(intent->request_relief) { return TEST_ACT_RELIEF; }
    if(intent->request_fire)   { return TEST_ACT_FIRE; }
  }
  return TEST_ACT_SAFE_OFF;
}

static void task_control_send_test_action(actuator_cmd_t *cmd,
    test_action_t test_action)
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

  app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd,
      (cmd->type == ACT_CMD_SAFE_OFF) ? pdTRUE : pdFALSE);
}

/**
 * @brief   测试模式主循环
 *
 * 安全锁处于 TEST MODE 时运行此分支。
 * 特点：
 *   - 不喷射燃料（锁油阀禁用）
 *   - 可通过按键或 DMX 执行动作测试
 *   - 菜单打开时强制安全关闭
 *   - 检测到致命故障时直接切换到 FAULT
 */
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

  /* 菜单打开时锁定所有输出 */
  if((s_task_control_cfg.ui_menu_active != 0) &&
     (*s_task_control_cfg.ui_menu_active == true))
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

  /* 致命故障立即停机 */
  if((in->latched_fault_mask & FAULT_MASK_FATAL) != 0U)
  {
    task_control_transition(app, MACHINE_FAULT, 0x2301U, now);
    cmd->type = ACT_CMD_SAFE_OFF;
    app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdTRUE);
    task_control_heartbeat_delay();
    return;
  }

  test_action = task_control_choose_test_action(
      key_menu, key_down, key_up, key_enter, bits, dmx_ok, intent);
  if(test_action != rt->last_test_action)
  {
    task_control_send_test_action(cmd, test_action);
    rt->last_test_action = test_action;
  }

  task_control_heartbeat_delay();
}

/**
 * @brief   用户模式主循环
 *
 * 安全锁处于 USER MODE 时运行此分支。
 *
 * ## 决策流程
 *   1. safety_guard_eval() 评估整体安全状态
 *   2. 根据评估结果执行对应动作
 *
 * ## 安全动作处理
 *   - SAFETY_FORCE_STOP  → 切换 FAULT，停止所有
 *   - SAFETY_LOCKED      → 切换 LOCKED，停止所有
 *   - SAFETY_FORCE_RELIEF→ 切换 RELIEF，开泄压阀
 *   - SAFETY_ALLOW_FIRE  → 根据 DMX 意向执行
 *     - relief 请求 → RELIEF
 *     - fire 请求 + 压力足够 → FIRING
 *     - fire 请求 + 压力不足 → READY，继续建压
 *     - 无请求 → 根据补压标志决定 PUMP / SAFE_OFF
 *
 * ## 自动补压
 *   压力低于 97% 时自动启动补压（pressure_refill_active=true），
 *   达到 100% 后停止。补压期间油泵运转但不喷射。
 */
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
      pressure_ready_for_fire =
          (pressure_pct >= CFG_PRESSURE_FIRE_MIN_PCT);

      /* 自动补压状态跟踪 */
      if(pressure_pct >= CFG_PRESSURE_TARGET_PCT)
      {
        rt->pressure_refill_active = false;
      }
      else if(pressure_pct <= CFG_PRESSURE_REFILL_RESUME_PCT)
      {
        rt->pressure_refill_active = true;
      }

      /* 根据 DMX 意向 + 压力状态执行 */
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
        /* 压力不够 → 建压但不喷射 */
        cmd->type = ACT_CMD_PUMP_ONLY;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
        task_control_transition(app, MACHINE_READY, 0x2008U, now);
      }
      else
      {
        cmd->type = rt->pressure_refill_active
                    ? ACT_CMD_PUMP_ONLY
                    : ACT_CMD_SAFE_OFF;
        app_task_queue_send_latest(s_task_control_cfg.q_actuator, cmd, pdFALSE);
        task_control_transition(app, MACHINE_READY, 0x2004U, now);
      }
      break;
  }
}

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
 * @brief   控制任务主循环
 *
 * ## 每个周期的处理步骤
 *   1. 处理 DMX 数据（edmx_rx_process）
 *   2. 更新 DMX 在线状态
 *   3. 解析 DMX → 执行意向
 *   4. 日志（值变化时输出）
 *   5. 读取传感器（压力）
 *   6. 安全评估
 *   7. 压力传感器故障检查
 *   8. 根据模式（测试/用户）执行对应逻辑
 *
 * ## 运行模式分流
 *   进入主循环后，根据安全锁状态永久分流：
 *   - user_mode == false → task_control_run_test_mode()
 *   - user_mode == true  → task_control_run_user_mode()
 *
 *   两种模式都包含自己的心跳延迟，不会在分流后返回公共代码。
 *   这是刻意设计：避免模式间状态污染，简化时序推理。
 */
void control_task(void *pvParameters)
{
  TickType_t now;
  EventBits_t bits;
  actuator_cmd_t cmd;
  dmx_intent_t intent;
  safety_eval_input_t in;
  bool ok;
  bool user_mode;
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  task_control_runtime_t rt;
  app_fsm_t *app;
  (void)pvParameters;

  rt.last_test_action = (test_action_t)0xFF;
  rt.pressure_refill_active = true;

  vTaskDelay(pdMS_TO_TICKS(50));
  task_control_run_startup_selftest();

  for(;;)
  {
    app = s_task_control_cfg.app;
    if((app == 0) ||
       (s_task_control_cfg.dmx_rx == 0) ||
       (s_task_control_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    now = xTaskGetTickCount();
    edmx_rx_process(s_task_control_cfg.dmx_rx, (uint32_t)now);
    task_control_update_dmx_online(s_task_control_cfg.dmx_rx,
                                   s_task_control_cfg.event_group, now);

    bits = xEventGroupGetBits(s_task_control_cfg.event_group);
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);

    ok = task_control_build_dmx_intent(app, s_task_control_cfg.dmx_rx,
                                       bits, &intent);
    task_control_dmx_business_log(app, s_task_control_cfg.dmx_rx,
                                  bits, ok, &intent);

    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    task_control_fill_safety_input(&in, app, bits, &intent,
                                   user_mode, pressure_pct);

    if(task_control_handle_pressure_sensor_fault(app, pressure_raw, now))
    {
      continue;
    }

    task_control_init_actuator_cmd(&cmd, app, &intent, user_mode);

    /* 测试模式 vs 用户模式分流 */
    if(user_mode == false)
    {
      task_control_run_test_mode(app, now, bits, &in, ok, &intent, &cmd, &rt);
      continue;
    }

    task_control_run_user_mode(app, now, pressure_pct, &intent, &in, &cmd, &rt);
    task_control_heartbeat_delay();
  }
}
