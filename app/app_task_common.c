#include "app_task_common.h"

void app_task_queue_send_latest(QueueHandle_t q, const actuator_cmd_t *cmd, BaseType_t to_front)
{
  actuator_cmd_t dropped;

  if((q == 0) || (cmd == 0))
  {
    return;
  }

  if(to_front != pdFALSE)
  {
    if(xQueueSendToFront(q, cmd, 0) == pdPASS)
    {
      return;
    }
  }
  else
  {
    if(xQueueSend(q, cmd, 0) == pdPASS)
    {
      return;
    }
  }

  (void)xQueueReceive(q, &dropped, 0);
  if(to_front != pdFALSE)
  {
    (void)xQueueSendToFront(q, cmd, 0);
  }
  else
  {
    (void)xQueueSend(q, cmd, 0);
  }
}

void app_task_set_state_bits(EventGroupHandle_t eg, machine_state_t st)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;

  if(eg == 0)
  {
    return;
  }

  clear_mask = EVT_STATE_READY_BIT | EVT_STATE_FIRING_BIT | EVT_STATE_RELIEF_BIT | EVT_STATE_FAULT_BIT | EVT_STATE_LOCKED_BIT;
  set_mask = 0U;

  switch(st)
  {
    case MACHINE_READY:
      set_mask = EVT_STATE_READY_BIT;
      break;
    case MACHINE_FIRING:
      set_mask = EVT_STATE_FIRING_BIT;
      break;
    case MACHINE_RELIEF:
      set_mask = EVT_STATE_RELIEF_BIT;
      break;
    case MACHINE_FAULT:
      set_mask = EVT_STATE_FAULT_BIT;
      break;
    case MACHINE_LOCKED:
      set_mask = EVT_STATE_LOCKED_BIT;
      break;
    default:
      break;
  }

  (void)xEventGroupClearBits(eg, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg, set_mask);
  }
}

void app_task_set_fault_bits(EventGroupHandle_t eg, uint32_t mask)
{
  EventBits_t clear_mask;
  EventBits_t set_mask;

  if(eg == 0)
  {
    return;
  }

  clear_mask = EVT_FAULT_E1_BIT | EVT_FAULT_E2_BIT | EVT_FAULT_E3_BIT | EVT_FAULT_E4_BIT | EVT_FAULT_E5_BIT;
  set_mask = 0U;
  if((mask & FAULT_MASK_E1) != 0U) { set_mask |= EVT_FAULT_E1_BIT; }
  if((mask & FAULT_MASK_E2) != 0U) { set_mask |= EVT_FAULT_E2_BIT; }
  if((mask & FAULT_MASK_E3) != 0U) { set_mask |= EVT_FAULT_E3_BIT; }
  if((mask & FAULT_MASK_E4) != 0U) { set_mask |= EVT_FAULT_E4_BIT; }
  if((mask & FAULT_MASK_E5) != 0U) { set_mask |= EVT_FAULT_E5_BIT; }

  (void)xEventGroupClearBits(eg, clear_mask);
  if(set_mask != 0U)
  {
    (void)xEventGroupSetBits(eg, set_mask);
  }
}

uint32_t app_task_read_fault_mask_from_events(EventBits_t bits)
{
  uint32_t mask;

  mask = 0U;
  if((bits & EVT_FAULT_E1_BIT) != 0U) { mask |= FAULT_MASK_E1; }
  if((bits & EVT_FAULT_E2_BIT) != 0U) { mask |= FAULT_MASK_E2; }
  if((bits & EVT_FAULT_E3_BIT) != 0U) { mask |= FAULT_MASK_E3; }
  if((bits & EVT_FAULT_E4_BIT) != 0U) { mask |= FAULT_MASK_E4; }
  if((bits & EVT_FAULT_E5_BIT) != 0U) { mask |= FAULT_MASK_E5; }
  return mask;
}

void app_task_send_safe_off_high_prio(QueueHandle_t q)
{
  actuator_cmd_t cmd;

  cmd.type = ACT_CMD_SAFE_OFF;
  cmd.priority = 10U;
  cmd.user_mode = false;
  cmd.igniter_delay_ms = 0U;
  cmd.oil_lock_delay_ms = 0U;
  cmd.fire_duration_ms = 0U;
  app_task_queue_send_latest(q, &cmd, pdTRUE);
}
