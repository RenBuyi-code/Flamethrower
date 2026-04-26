#include "app_core.h"
#include "log_rtt.h"

/*
 * Keep app_core intentionally thin.
 *
 * app_core gathers shared application state,
 * while domain rules stay in domain/ and task code.
 *
 * That keeps:
 * - one clear system truth
 * - one small debug entry point
 * - no accidental UI-specific logic inside the app core
 */

void app_core_init(app_core_t *core)
{
  if(core == 0)
  {
    return;
  }

  bsp_at32f415_bind(&core->hal);
  machine_state_init(&core->machine);
  fault_manager_init(&core->faults);
  event_log_init(&core->events);
  cfg_get_default_params(&core->params);
}

void app_core_load_or_default_params(app_core_t *core)
{
  if(core == 0)
  {
    return;
  }

  if(core->hal.storage.load_params(core->hal.storage.ctx, &core->params) == false)
  {
    cfg_get_default_params(&core->params);
    APP_LOGW("params load failed, use defaults");
  }

  cfg_sanitize_params(&core->params);
  APP_LOGI("params: addr=%u mode=%u ign=%u lock=%u tilt=%u",
           (unsigned)core->params.dmx_address,
           (unsigned)core->params.dmx_mode,
           (unsigned)core->params.igniter_delay_ms,
           (unsigned)core->params.oil_lock_delay_ms,
           (unsigned)core->params.tilt_protect_enable);
}

void app_core_log(app_core_t *core, uint16_t code, uint32_t ts_ms)
{
  if(core == 0)
  {
    return;
  }

  event_log_push(&core->events, code, ts_ms);
}

bool app_core_switch_state(app_core_t *core, machine_state_t next, uint16_t event_code, uint32_t ts_ms)
{
  bool ok;
  machine_state_t from;

  if(core == 0)
  {
    return false;
  }

  from = core->machine.current;
  if(from == next)
  {
    return true;
  }

  ok = machine_state_transition(&core->machine, next, event_code);
  if(ok)
  {
    app_core_log(core, event_code, ts_ms);
    APP_LOGI("state %u -> %u ev=0x%04X",
             (unsigned)from,
             (unsigned)next,
             (unsigned)event_code);
  }
  else
  {
    app_core_log(core, (uint16_t)(0xF000U | event_code), ts_ms);
    APP_LOGW("illegal transition state=%u to=%u ev=0x%04X",
             (unsigned)from,
             (unsigned)next,
             (unsigned)event_code);
  }

  return ok;
}
