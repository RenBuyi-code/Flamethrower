#ifndef UI_SERVICES_H
#define UI_SERVICES_H

#include "rules/state_machine.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * ui_services
 *
 * Project-level UI adapter for the Flamethrower application.
 * This is not part of SlateUI itself.
 *
 * It gives pages:
 * - one small readonly runtime snapshot
 * - one small write path for settings commits
 *
 * That keeps page code away from g_app, HAL details and task internals.
 * Another project can use SlateUI without copying this file.
 */

typedef struct
{
  machine_state_t state;
  uint8_t pressure_pct;
  uint16_t dmx_addr;
  uint32_t fault_mask;
  bool dmx_online;
  bool pumping;
} ui_machine_snapshot_t;

typedef struct
{
  void (*save_dmx_addr)(int16_t value);
  void (*save_dmx_mode)(int16_t value);
  void (*save_ign_delay)(int16_t value);
  void (*save_lock_delay)(int16_t value);
  void (*save_tilt_enable)(int16_t value);
  void (*save_language)(int16_t value);
} ui_setting_handlers_t;

bool ui_service_set_machine_snapshot(const ui_machine_snapshot_t *snapshot);
bool ui_service_get_machine_snapshot(ui_machine_snapshot_t *out);

void ui_service_bind_setting_handlers(const ui_setting_handlers_t *handlers);

void ui_service_save_dmx_addr(int16_t value);
void ui_service_save_dmx_mode(int16_t value);
void ui_service_save_ign_delay(int16_t value);
void ui_service_save_lock_delay(int16_t value);
void ui_service_save_tilt_enable(int16_t value);
void ui_service_save_language(int16_t value);

#endif
