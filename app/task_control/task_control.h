#ifndef APP_TASK_CONTROL_H
#define APP_TASK_CONTROL_H

#include "../../project/inc/freertos_app.h"
#include "../app_core.h"
#include "../app_task_shared.h"
#include "../../middleware/easyDMX/easy_dmx.h"

typedef struct
{
  app_core_t *app;
  edmx_rx_t *dmx_rx;
  QueueHandle_t q_actuator;
  EventGroupHandle_t event_group;
  EventBits_t hb_bit;
  volatile bool *ui_menu_active;
} app_task_control_cfg_t;

void app_task_control_init(const app_task_control_cfg_t *cfg);
void control_task(void *pvParameters);

#endif
