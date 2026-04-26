#include "safety_guard.h"
#include "fault_manager.h"

safety_action_t safety_guard_eval(const safety_eval_input_t *in)
{
  if(in == 0)
  {
    return SAFETY_FORCE_STOP;
  }

  if((in->latched_fault_mask & FAULT_MASK_FATAL) != 0U)
  {
    return SAFETY_FORCE_STOP;
  }

  if(in->in_user_mode == 0)
  {
    return SAFETY_LOCKED;
  }

  if(in->relief_requested != 0)
  {
    return SAFETY_FORCE_RELIEF;
  }

  if(in->tilt_fault_active != 0)
  {
    return SAFETY_FORCE_STOP;
  }

  if(in->voltage_ok == 0)
  {
    return SAFETY_FORCE_STOP;
  }

  return SAFETY_ALLOW_FIRE;
}
