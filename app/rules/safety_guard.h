#ifndef APP_RULES_SAFETY_GUARD_H
#define APP_RULES_SAFETY_GUARD_H

#include <stdint.h>

typedef enum
{
  SAFETY_ALLOW_FIRE = 0,
  SAFETY_FORCE_STOP,
  SAFETY_FORCE_RELIEF,
  SAFETY_LOCKED
} safety_action_t;

typedef struct
{
  uint32_t latched_fault_mask;
  int dmx_online;
  int relief_requested;
  int fire_requested;
  int in_user_mode;
  int tilt_fault_active;
  int voltage_ok;
  uint8_t pressure_pct;
  uint8_t pressure_fire_min_pct;
} safety_eval_input_t;

safety_action_t safety_guard_eval(const safety_eval_input_t *in);

#endif
