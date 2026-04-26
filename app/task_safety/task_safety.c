#include "task_safety.h"
#include "../app_task_common.h"
#include "../log_rtt.h"
#include "../../cfg/system_config.h"
#include "../../domain/fault_manager.h"
#include <string.h>

static app_task_safety_cfg_t s_task_safety_cfg;

void app_task_safety_init(const app_task_safety_cfg_t *cfg)
{
  if(cfg == 0)
  {
    memset(&s_task_safety_cfg, 0, sizeof(s_task_safety_cfg));
    return;
  }

  s_task_safety_cfg = *cfg;
}

void safety_task(void *pvParameters)
{
  uint16_t pressure_raw;
  uint8_t pressure_pct;
  uint16_t voltage_raw;
  bool user_mode;
  bool tilt_fault;
  TickType_t now;
  TickType_t e1_start;
  TickType_t e3_start;
  TickType_t e5_start;
  uint32_t last_fault_mask;
  actuator_status_t st;
  BaseType_t have_status;
  EventBits_t bits;
  app_core_t *app;
  (void)pvParameters;

  e1_start = 0U;
  e3_start = 0U;
  e5_start = 0U;
  last_fault_mask = 0U;

  for(;;)
  {
    app = s_task_safety_cfg.app;
    if((app == 0) || (s_task_safety_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    now = xTaskGetTickCount();
    pressure_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_PRESSURE);
    pressure_pct = cfg_pressure_raw_to_percent(pressure_raw);
    voltage_raw = app->hal.adc.read_raw(app->hal.adc.ctx, SENSOR_POWER1);
    user_mode = app->hal.input.read(app->hal.input.ctx, INPUT_SAFETY_LOCK);
    tilt_fault = app->hal.input.read(app->hal.input.ctx, INPUT_TILT_SWITCH);

    have_status = xQueuePeek(s_task_safety_cfg.q_actuator_status, &st, 0);
    bits = xEventGroupGetBits(s_task_safety_cfg.event_group);

    if(cfg_pressure_sensor_fault(pressure_raw))
    {
      e1_start = 0U;
      fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      APP_LOGW("pressure sensor fault: raw=%u", (unsigned)pressure_raw);
    }
    else if((have_status == pdTRUE) &&
            st.out.oil_pump_on &&
            (st.fire_active == false) &&
            (st.relief_active == false) &&
            (pressure_pct < CFG_PRESSURE_TARGET_PCT))
    {
      if(e1_start == 0U)
      {
        e1_start = now;
      }
      else if((now - e1_start) >= pdMS_TO_TICKS(CFG_PRESSURE_ERROR_TIMEOUT_MS))
      {
        fault_manager_set(&app->faults, FAULT_E1_PRESSURE_BUILD);
      }
    }
    else
    {
      e1_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E1_PRESSURE_BUILD,
                              (cfg_pressure_sensor_fault(pressure_raw) == false) &&
                              (pressure_pct >= CFG_PRESSURE_TARGET_PCT));
    }

    if(app->params.tilt_protect_enable && tilt_fault)
    {
      fault_manager_set(&app->faults, FAULT_E2_TILT);
    }
    else
    {
      fault_manager_try_clear(&app->faults, FAULT_E2_TILT, true);
    }

    if(cfg_voltage_raw_in_range(voltage_raw) == false)
    {
      if(e3_start == 0U)
      {
        e3_start = now;
      }
      else if((now - e3_start) >= pdMS_TO_TICKS(CFG_VOLTAGE_ERROR_HOLD_MS))
      {
        fault_manager_set(&app->faults, FAULT_E3_VOLTAGE);
      }
    }
    else
    {
      e3_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E3_VOLTAGE, true);
    }

    if((user_mode == false) && (app->machine.current != MACHINE_SELFTEST))
    {
      fault_manager_set(&app->faults, FAULT_E4_LOCKED_MODE);
    }
    else
    {
      fault_manager_try_clear(&app->faults, FAULT_E4_LOCKED_MODE, true);
    }

    if((have_status == pdTRUE) && st.out.relief_valve_on && (pressure_pct > CFG_PRESSURE_RELIEF_DONE_PCT))
    {
      if(e5_start == 0U)
      {
        e5_start = now;
      }
      else if((now - e5_start) >= pdMS_TO_TICKS(CFG_RELIEF_ERROR_TIMEOUT_MS))
      {
        fault_manager_set(&app->faults, FAULT_E5_RELIEF);
      }
    }
    else
    {
      e5_start = 0U;
      fault_manager_try_clear(&app->faults, FAULT_E5_RELIEF, pressure_pct <= CFG_PRESSURE_RELIEF_DONE_PCT);
    }

    app_task_set_fault_bits(s_task_safety_cfg.event_group, app->faults.latched_mask);
    if(last_fault_mask != app->faults.latched_mask)
    {
      APP_LOGW("fault mask: 0x%02lX -> 0x%02lX",
               (unsigned long)last_fault_mask,
               (unsigned long)app->faults.latched_mask);
      last_fault_mask = app->faults.latched_mask;
    }

    if((app->faults.latched_mask & FAULT_MASK_FATAL) != 0U)
    {
      app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
      (void)app_core_switch_state(app, MACHINE_FAULT, 0x2101U, (uint32_t)now);
      app_task_set_state_bits(s_task_safety_cfg.event_group, app->machine.current);
    }
    else if((app->faults.latched_mask & FAULT_MASK_E4) != 0U)
    {
      (void)app_core_switch_state(app, MACHINE_LOCKED, 0x2102U, (uint32_t)now);
      app_task_set_state_bits(s_task_safety_cfg.event_group, app->machine.current);
    }

    if((user_mode == true) && ((bits & EVT_DMX_ONLINE_BIT) == 0U))
    {
      app_task_send_safe_off_high_prio(s_task_safety_cfg.q_actuator);
    }

    (void)xEventGroupSetBits(s_task_safety_cfg.event_group, s_task_safety_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
