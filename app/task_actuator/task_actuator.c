/**
 * @file    task_actuator.c
 * @brief   执行器控制任务
 *
 * ## 职责
 *   接收控制任务发来的命令（ACT_CMD_*），驱动物理执行器：
 *   - 油泵、锁油阀、点火器、泄压阀
 *   - 状态指示灯（报错、油泵、DMX、电源、模式）
 *
 * ## 点火时序
 *   收到 ACT_CMD_FIRE 后，按以下时间线执行：
 *
 *   T=0       油泵启动，建立油压
 *   T=ign     点火器通电（igniter_delay_sec）
 *   T=lock    锁油阀打开（oil_lock_delay_sec，仅用户模式）
 *   T=dur     定时结束，关闭点火器和锁油阀（fire_duration_ms > 0 时）
 *
 *   点火器与锁油阀之间的延时差，用于让油泵先建压，
 *   再点火，最后开阀喷火，避免"冷喷"或"爆燃"。
 *
 * ## 安全设计
 *   - TIMED 模式（fire_duration_ms > 0）：到达时长自动关阀
 *   - PERMANENT 模式（fire_duration_ms == 0）：持续到外部发 SAFE_OFF
 *   - RELIEF 模式：只开泄压阀，其余全关
 *   - 每周期将执行器状态写入队列，供安全任务监控
 */

#include "task_actuator.h"

static app_task_actuator_cfg_t s_task_actuator_cfg;

void app_task_actuator_init(const app_task_actuator_cfg_t *cfg)
{
  if(cfg == 0)
  {
    s_task_actuator_cfg.app = 0;
    s_task_actuator_cfg.q_actuator = 0;
    s_task_actuator_cfg.q_actuator_status = 0;
    s_task_actuator_cfg.event_group = 0;
    s_task_actuator_cfg.hb_bit = 0U;
    s_task_actuator_cfg.led_error_mask = 0U;
    s_task_actuator_cfg.dmx_online_bit = 0U;
    return;
  }

  s_task_actuator_cfg = *cfg;
}

void actuator_task(void *pvParameters)
{
  actuator_output_t out;
  actuator_cmd_t cmd;
  BaseType_t got;
  TickType_t fire_start_tick;
  bool fire_active;
  bool relief_active;
  actuator_status_t status;
  TickType_t now;
  bool enable_igniter;
  bool enable_lock_valve;
  bool user_mode_latched;
  bool user_mode_latched_ready;
  TickType_t elapsed;
  EventBits_t bits;
  (void)pvParameters;

  /* ---- 初始状态：全部关闭 ---- */
  out.oil_pump_on = false;
  out.oil_lock_valve_on = false;
  out.relief_valve_on = false;
  out.igniter_on = false;
  out.led_error_on = false;
  out.led_oil_pump_on = false;
  out.led_dmx_on = false;
  out.led_power_on = true;         /* 电源指示灯常亮 */
  out.led_mode_on = false;

  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 0U;
  cmd.user_mode = false;
  cmd.igniter_delay_sec = 0U;
  cmd.oil_lock_delay_sec = 0U;
  cmd.fire_duration_ms = 0U;

  fire_start_tick = 0U;
  fire_active = false;
  relief_active = false;
  user_mode_latched = false;
  user_mode_latched_ready = false;

  for(;;)
  {
    /* 配置未就绪时等待 */
    if((s_task_actuator_cfg.app == 0) ||
       (s_task_actuator_cfg.q_actuator == 0) ||
       (s_task_actuator_cfg.q_actuator_status == 0) ||
       (s_task_actuator_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    /* 启动时 latch 一次安全锁状态，之后由命令更新 */
    if(user_mode_latched_ready == false)
    {
      user_mode_latched = s_task_actuator_cfg.app->hal.input.read(
          s_task_actuator_cfg.app->hal.input.ctx, INPUT_SAFETY_LOCK);
      user_mode_latched_ready = true;
    }

    /* 阻塞等待命令，10ms 超时用于定期刷新状态 */
    got = xQueueReceive(s_task_actuator_cfg.q_actuator, &cmd, pdMS_TO_TICKS(10));
    now = xTaskGetTickCount();

    if(got == pdTRUE)
    {
      user_mode_latched = cmd.user_mode;
      switch(cmd.type)
      {
        case ACT_CMD_SAFE_OFF:
          /* 全机关闭：停止所有执行器 */
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_RELIEF:
          /* 泄压模式：只开泄压阀，油泵/点火器/锁油阀全部关闭 */
          fire_active = false;
          relief_active = true;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = true;
          out.igniter_on = false;
          break;

        case ACT_CMD_PUMP_ONLY:
          /* 仅建压：油泵运转但不点火、不开阀 */
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = true;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_FIRE:
        default:
          /* 点火命令：记录起始时刻，先启泵建压 */
          fire_active = true;
          relief_active = false;
          fire_start_tick = now;
          out.oil_pump_on = true;
          out.relief_valve_on = false;
          out.oil_lock_valve_on = false;
          out.igniter_on = false;
          break;
      }
    }

    /* ========== 喷射时序控制 ========== */
    if(fire_active)
    {
      elapsed = now - fire_start_tick;

      /* 点火器延时到达？单位来自系统参数（秒），转 tick 需 ×1000 */
      enable_igniter = (elapsed >= pdMS_TO_TICKS(cmd.igniter_delay_sec * 1000U));
      /* 锁油阀延时到达？（仅用户模式才开阀喷油） */
      enable_lock_valve = cmd.user_mode &&
                         (elapsed >= pdMS_TO_TICKS(cmd.oil_lock_delay_sec * 1000U));

      out.oil_pump_on = true;
      out.relief_valve_on = false;
      out.igniter_on = enable_igniter;
      out.oil_lock_valve_on = enable_lock_valve;

      /* 定时喷射到期 → 自动关阀停火 */
      if((cmd.fire_duration_ms > 0U) &&
         (elapsed >= pdMS_TO_TICKS(cmd.fire_duration_ms)))
      {
        fire_active = false;
        out.oil_lock_valve_on = false;
        out.igniter_on = false;
      }
    }

    /* ========== 泄压状态保持 ========== */
    if(relief_active)
    {
      out.relief_valve_on = true;
      out.oil_pump_on = false;
      out.oil_lock_valve_on = false;
      out.igniter_on = false;
    }

    /* ========== LED 指示灯更新 ========== */
    bits = xEventGroupGetBits(s_task_actuator_cfg.event_group);
    out.led_oil_pump_on = out.oil_pump_on;
    out.led_error_on = ((bits & s_task_actuator_cfg.led_error_mask) != 0U);
    out.led_dmx_on = ((bits & s_task_actuator_cfg.dmx_online_bit) != 0U);
    out.led_power_on = true;
    out.led_mode_on = user_mode_latched;

    /* 刷写物理输出 */
    s_task_actuator_cfg.app->hal.actuator.apply(
        s_task_actuator_cfg.app->hal.actuator.ctx, &out);

    /* 发布状态快照（覆盖式，供安全任务和 UI 读取） */
    status.out = out;
    status.tick_ms = (uint32_t)now;
    status.fire_active = fire_active;
    status.relief_active = relief_active;
    (void)xQueueOverwrite(s_task_actuator_cfg.q_actuator_status, &status);

    (void)xEventGroupSetBits(s_task_actuator_cfg.event_group,
                             s_task_actuator_cfg.hb_bit);
  }
}
