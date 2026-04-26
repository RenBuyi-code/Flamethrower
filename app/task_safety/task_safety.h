#ifndef APP_TASK_SAFETY_H
#define APP_TASK_SAFETY_H

#include "../../project/inc/freertos_app.h"
#include "../app_core.h"
#include "../app_task_shared.h"

typedef struct
{
  app_core_t *app;
  QueueHandle_t q_actuator;
  QueueHandle_t q_actuator_status;
  EventGroupHandle_t event_group;
  EventBits_t hb_bit;
} app_task_safety_cfg_t;

void app_task_safety_init(const app_task_safety_cfg_t *cfg);
void safety_task(void *pvParameters);

#endif
