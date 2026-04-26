#ifndef APP_APP_CORE_H
#define APP_APP_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "../bsp/at32f415/bsp_at32f415.h"
#include "../cfg/system_config.h"
#include "../domain/event_log.h"
#include "../domain/fault_manager.h"
#include "../domain/machine_state.h"

typedef struct
{
  bsp_hal_bundle_t hal;
  machine_state_ctx_t machine;
  fault_manager_t faults;
  event_log_t events;
  system_params_t params;
} app_core_t;

void app_core_init(app_core_t *core);
void app_core_load_or_default_params(app_core_t *core);
void app_core_log(app_core_t *core, uint16_t code, uint32_t ts_ms);
bool app_core_switch_state(app_core_t *core, machine_state_t next, uint16_t event_code, uint32_t ts_ms);

#endif
