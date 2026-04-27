#ifndef APP_TASK_SHARED_H
#define APP_TASK_SHARED_H

#include "../project/inc/freertos_app.h"
#include "../bsp/at32f415/bsp_at32f415.h"
#include "rules/state_machine.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  ACT_CMD_SAFE_OFF = 0,
  ACT_CMD_RELIEF,
  ACT_CMD_FIRE,
  ACT_CMD_PUMP_ONLY
} actuator_cmd_type_t;

typedef struct
{
  actuator_cmd_type_t type;
  uint8_t priority;
  bool user_mode;
  uint16_t igniter_delay_ms;
  uint16_t oil_lock_delay_ms;
  uint16_t fire_duration_ms;
} actuator_cmd_t;

typedef struct
{
  actuator_output_t out;
  uint32_t tick_ms;
  bool fire_active;
  bool relief_active;
} actuator_status_t;

enum
{
  EVT_STATE_READY_BIT = (1UL << 0),
  EVT_STATE_FIRING_BIT = (1UL << 1),
  EVT_STATE_RELIEF_BIT = (1UL << 2),
  EVT_STATE_FAULT_BIT = (1UL << 3),
  EVT_STATE_LOCKED_BIT = (1UL << 4),

  EVT_DMX_ONLINE_BIT = (1UL << 5),
  EVT_FAULT_E1_BIT = (1UL << 6),
  EVT_FAULT_E2_BIT = (1UL << 7),
  EVT_FAULT_E3_BIT = (1UL << 8),
  EVT_FAULT_E4_BIT = (1UL << 9),
  EVT_FAULT_E5_BIT = (1UL << 10),

  EVT_HB_SAFETY_BIT = (1UL << 11),
  EVT_HB_CONTROL_BIT = (1UL << 12),
  EVT_HB_ACTUATOR_BIT = (1UL << 13),
  EVT_HB_DMX_BIT = (1UL << 14),
  EVT_HB_UI_BIT = (1UL << 15),
  EVT_HB_DIAG_BIT = (1UL << 16),

  EVT_HB_MASK = EVT_HB_SAFETY_BIT | EVT_HB_CONTROL_BIT | EVT_HB_ACTUATOR_BIT | EVT_HB_DMX_BIT | EVT_HB_UI_BIT
};

typedef enum
{
  TEST_ACT_SAFE_OFF = 0,
  TEST_ACT_PUMP_ONLY,
  TEST_ACT_RELIEF,
  TEST_ACT_FIRE
} test_action_t;

#endif
