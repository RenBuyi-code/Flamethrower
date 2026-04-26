#ifndef APP_TASK_COMMON_H
#define APP_TASK_COMMON_H

#include "app_task_shared.h"
#include "../domain/fault_manager.h"

void app_task_queue_send_latest(QueueHandle_t q, const actuator_cmd_t *cmd, BaseType_t to_front);
void app_task_set_state_bits(EventGroupHandle_t eg, machine_state_t st);
void app_task_set_fault_bits(EventGroupHandle_t eg, uint32_t mask);
uint32_t app_task_read_fault_mask_from_events(EventBits_t bits);
void app_task_send_safe_off_high_prio(QueueHandle_t q);

#endif
