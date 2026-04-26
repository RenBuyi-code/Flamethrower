#include "ui_services.h"
#include <string.h>

/*
 * Keep this file intentionally small.
 *
 * Today it provides:
 * - one readonly machine snapshot for display pages
 * - one small settings commit adapter for editor pages
 *
 * If the application grows, this can split into more focused services.
 */

static ui_machine_snapshot_t s_machine_snapshot;
static bool s_machine_snapshot_valid;
static ui_setting_handlers_t s_setting_handlers;

bool ui_service_set_machine_snapshot(const ui_machine_snapshot_t *snapshot)
{
  bool changed;

  if(snapshot == 0)
  {
    return false;
  }

  changed = (s_machine_snapshot_valid == false) ||
            (memcmp(&s_machine_snapshot, snapshot, sizeof(s_machine_snapshot)) != 0);

  s_machine_snapshot = *snapshot;
  s_machine_snapshot_valid = true;
  return changed;
}

bool ui_service_get_machine_snapshot(ui_machine_snapshot_t *out)
{
  if((out == 0) || (s_machine_snapshot_valid == false))
  {
    return false;
  }

  *out = s_machine_snapshot;
  return true;
}

void ui_service_bind_setting_handlers(const ui_setting_handlers_t *handlers)
{
  if(handlers == 0)
  {
    memset(&s_setting_handlers, 0, sizeof(s_setting_handlers));
    return;
  }

  s_setting_handlers = *handlers;
}

void ui_service_save_dmx_addr(int16_t value)
{
  if(s_setting_handlers.save_dmx_addr != 0)
  {
    s_setting_handlers.save_dmx_addr(value);
  }
}

void ui_service_save_dmx_mode(int16_t value)
{
  if(s_setting_handlers.save_dmx_mode != 0)
  {
    s_setting_handlers.save_dmx_mode(value);
  }
}

void ui_service_save_ign_delay(int16_t value)
{
  if(s_setting_handlers.save_ign_delay != 0)
  {
    s_setting_handlers.save_ign_delay(value);
  }
}

void ui_service_save_lock_delay(int16_t value)
{
  if(s_setting_handlers.save_lock_delay != 0)
  {
    s_setting_handlers.save_lock_delay(value);
  }
}

void ui_service_save_tilt_enable(int16_t value)
{
  if(s_setting_handlers.save_tilt_enable != 0)
  {
    s_setting_handlers.save_tilt_enable(value);
  }
}

void ui_service_save_language(int16_t value)
{
  if(s_setting_handlers.save_language != 0)
  {
    s_setting_handlers.save_language(value);
  }
}
