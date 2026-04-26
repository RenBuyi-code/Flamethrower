#ifndef APP_TASK_DMX_H
#define APP_TASK_DMX_H

#include "../../project/inc/freertos_app.h"
#include "../app_core.h"
#include "../../middleware/easyDMX/easy_dmx.h"

typedef struct
{
  app_core_t *app;
  edmx_rx_t *rx;
  EventGroupHandle_t event_group;
  EventBits_t hb_bit;
} app_task_dmx_cfg_t;

void app_task_dmx_init(const app_task_dmx_cfg_t *cfg);
void dmx_task(void *pvParameters);

#endif
