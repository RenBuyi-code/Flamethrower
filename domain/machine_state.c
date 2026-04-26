#include "machine_state.h"

static bool can_move(machine_state_t from, machine_state_t to)
{
  switch(from)
  {
    case MACHINE_BOOT:
      return (to == MACHINE_SELFTEST);
    case MACHINE_SELFTEST:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED);
    case MACHINE_READY:
      return (to == MACHINE_SELFTEST) || (to == MACHINE_FIRING) || (to == MACHINE_RELIEF) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED);
    case MACHINE_FIRING:
      return (to == MACHINE_READY) || (to == MACHINE_RELIEF) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_RELIEF:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_FAULT:
      return (to == MACHINE_READY) || (to == MACHINE_LOCKED) || (to == MACHINE_SELFTEST);
    case MACHINE_LOCKED:
      return (to == MACHINE_READY) || (to == MACHINE_FAULT) || (to == MACHINE_SELFTEST);
    default:
      return false;
  }
}

void machine_state_init(machine_state_ctx_t *ctx)
{
  if(ctx == 0)
  {
    return;
  }
  ctx->current = MACHINE_BOOT;
  ctx->last_event = 0U;
  ctx->transition_count = 0U;
}

bool machine_state_transition(machine_state_ctx_t *ctx, machine_state_t to, uint16_t event_code)
{
  if(ctx == 0)
  {
    return false;
  }

  if(can_move(ctx->current, to) == false)
  {
    return false;
  }

  ctx->current = to;
  ctx->last_event = event_code;
  ctx->transition_count++;
  return true;
}
