#ifndef DOMAIN_MACHINE_STATE_H
#define DOMAIN_MACHINE_STATE_H

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
} machine_state_ctx_t;

void machine_state_init(machine_state_ctx_t *ctx);
bool machine_state_transition(machine_state_ctx_t *ctx, machine_state_t to, uint16_t event_code);

#endif
