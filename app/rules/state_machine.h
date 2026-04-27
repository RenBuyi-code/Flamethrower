#ifndef APP_RULES_STATE_MACHINE_H
#define APP_RULES_STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  MACHINE_BOOT = 0,
  MACHINE_SELFTEST,
  MACHINE_READY,
  MACHINE_FIRING,
  MACHINE_RELIEF,
  MACHINE_FAULT,
  MACHINE_LOCKED
} machine_state_t;

typedef struct
{
  machine_state_t current;
  uint16_t last_event;
  uint32_t transition_count;
} state_machine_t;

void state_machine_init(state_machine_t *ctx);
bool state_machine_transition(state_machine_t *ctx, machine_state_t to, uint16_t event_code);

#endif
