#ifndef APP_TASK_UI_H
#define APP_TASK_UI_H

#include "../../project/inc/freertos_app.h"
#include "../app_fsm.h"
#include "../app_task_shared.h"

typedef struct
{
  app_fsm_t *app;
  QueueHandle_t q_actuator_status;
  EventGroupHandle_t event_group;
  EventBits_t dmx_online_bit;
  EventBits_t hb_bit;
  volatile bool *menu_active;
} app_task_ui_cfg_t;

void app_task_ui_init(const app_task_ui_cfg_t *cfg);
void ui_task(void *pvParameters);

#endif
