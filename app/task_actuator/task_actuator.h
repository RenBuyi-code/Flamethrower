#ifndef APP_TASK_ACTUATOR_H
#define APP_TASK_ACTUATOR_H

#include "../../project/inc/freertos_app.h"
#include "../app_fsm.h"
#include "../app_task_shared.h"

typedef struct
{
  app_fsm_t *app;
  QueueHandle_t q_actuator;
  QueueHandle_t q_actuator_status;
  EventGroupHandle_t event_group;
  EventBits_t hb_bit;
  EventBits_t led_error_mask;
  EventBits_t dmx_online_bit;
} app_task_actuator_cfg_t;

void app_task_actuator_init(const app_task_actuator_cfg_t *cfg);
void actuator_task(void *pvParameters);

#endif
