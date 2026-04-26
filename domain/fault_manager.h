#ifndef DOMAIN_FAULT_MANAGER_H
#define DOMAIN_FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  FAULT_E1_PRESSURE_BUILD = 0,
  FAULT_E2_TILT = 1,
  FAULT_E3_VOLTAGE = 2,
  FAULT_E4_LOCKED_MODE = 3,
  FAULT_E5_RELIEF = 4
} fault_code_t;

#define FAULT_MASK_E1   (1UL << FAULT_E1_PRESSURE_BUILD)
#define FAULT_MASK_E2   (1UL << FAULT_E2_TILT)
#define FAULT_MASK_E3   (1UL << FAULT_E3_VOLTAGE)
#define FAULT_MASK_E4   (1UL << FAULT_E4_LOCKED_MODE)
#define FAULT_MASK_E5   (1UL << FAULT_E5_RELIEF)

#define FAULT_MASK_FATAL   (FAULT_MASK_E1 | FAULT_MASK_E2 | FAULT_MASK_E3 | FAULT_MASK_E5)

typedef struct
{
  uint32_t latched_mask;
} fault_manager_t;

void fault_manager_init(fault_manager_t *fm);
void fault_manager_set(fault_manager_t *fm, fault_code_t code);
void fault_manager_try_clear(fault_manager_t *fm, fault_code_t code, bool clear_condition_met);
bool fault_manager_is_latched(const fault_manager_t *fm, fault_code_t code);

#endif
