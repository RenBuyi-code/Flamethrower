#include "fault_manager.h"

void fault_manager_init(fault_manager_t *fm)
{
  if(fm == 0)
  {
    return;
  }
  fm->latched_mask = 0U;
}

void fault_manager_set(fault_manager_t *fm, fault_code_t code)
{
  if(fm == 0)
  {
    return;
  }
  fm->latched_mask |= (1UL << (uint32_t)code);
}

void fault_manager_try_clear(fault_manager_t *fm, fault_code_t code, bool clear_condition_met)
{
  if((fm == 0) || (clear_condition_met == false))
  {
    return;
  }
  fm->latched_mask &= ~(1UL << (uint32_t)code);
}

bool fault_manager_is_latched(const fault_manager_t *fm, fault_code_t code)
{
  uint32_t mask;
  if(fm == 0)
  {
    return false;
  }
  mask = (1UL << (uint32_t)code);
  return ((fm->latched_mask & mask) != 0U);
}
