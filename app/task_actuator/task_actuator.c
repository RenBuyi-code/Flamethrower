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

  out.oil_pump_on = false;
  out.oil_lock_valve_on = false;
  out.relief_valve_on = false;
  out.igniter_on = false;
  out.led_error_on = false;
  out.led_oil_pump_on = false;
  out.led_dmx_on = false;
  out.led_power_on = true;
  out.led_mode_on = false;

  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 0U;
  cmd.user_mode = false;
  cmd.igniter_delay_ms = 0U;
  cmd.oil_lock_delay_ms = 0U;
  cmd.fire_duration_ms = 0U;

  fire_start_tick = 0U;
  fire_active = false;
  relief_active = false;
  user_mode_latched = false;
  user_mode_latched_ready = false;

  for(;;)
  {
    if((s_task_actuator_cfg.app == 0) ||
       (s_task_actuator_cfg.q_actuator == 0) ||
       (s_task_actuator_cfg.q_actuator_status == 0) ||
       (s_task_actuator_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if(user_mode_latched_ready == false)
    {
      user_mode_latched = s_task_actuator_cfg.app->hal.input.read(s_task_actuator_cfg.app->hal.input.ctx, INPUT_SAFETY_LOCK);
      user_mode_latched_ready = true;
    }

    got = xQueueReceive(s_task_actuator_cfg.q_actuator, &cmd, pdMS_TO_TICKS(10));
    now = xTaskGetTickCount();

    if(got == pdTRUE)
    {
      user_mode_latched = cmd.user_mode;
      switch(cmd.type)
      {
        case ACT_CMD_SAFE_OFF:
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_RELIEF:
          fire_active = false;
          relief_active = true;
          out.oil_pump_on = false;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = true;
          out.igniter_on = false;
          break;

        case ACT_CMD_PUMP_ONLY:
          fire_active = false;
          relief_active = false;
          out.oil_pump_on = true;
          out.oil_lock_valve_on = false;
          out.relief_valve_on = false;
          out.igniter_on = false;
          break;

        case ACT_CMD_FIRE:
        default:
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

    if(fire_active)
    {
      elapsed = now - fire_start_tick;
      enable_igniter = (elapsed >= pdMS_TO_TICKS(cmd.igniter_delay_ms));
      enable_lock_valve = cmd.user_mode && (elapsed >= pdMS_TO_TICKS(cmd.oil_lock_delay_ms));

      out.oil_pump_on = true;
      out.relief_valve_on = false;
      out.igniter_on = enable_igniter;
      out.oil_lock_valve_on = enable_lock_valve;

      if((cmd.fire_duration_ms > 0U) && (elapsed >= pdMS_TO_TICKS(cmd.fire_duration_ms)))
      {
        fire_active = false;
        out.oil_lock_valve_on = false;
        out.igniter_on = false;
      }
    }

    if(relief_active)
    {
      out.relief_valve_on = true;
      out.oil_pump_on = false;
      out.oil_lock_valve_on = false;
      out.igniter_on = false;
    }

    bits = xEventGroupGetBits(s_task_actuator_cfg.event_group);
    out.led_oil_pump_on = out.oil_pump_on;
    out.led_error_on = ((bits & s_task_actuator_cfg.led_error_mask) != 0U);
    out.led_dmx_on = ((bits & s_task_actuator_cfg.dmx_online_bit) != 0U);
    out.led_power_on = true;
    out.led_mode_on = user_mode_latched;

    s_task_actuator_cfg.app->hal.actuator.apply(s_task_actuator_cfg.app->hal.actuator.ctx, &out);

    status.out = out;
    status.tick_ms = (uint32_t)now;
    status.fire_active = fire_active;
    status.relief_active = relief_active;
    (void)xQueueOverwrite(s_task_actuator_cfg.q_actuator_status, &status);

    (void)xEventGroupSetBits(s_task_actuator_cfg.event_group, s_task_actuator_cfg.hb_bit);
  }
}
