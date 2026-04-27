#ifndef APP_TASK_DIAG_H
#define APP_TASK_DIAG_H

#include "../../project/inc/freertos_app.h"

typedef struct
{
  QueueHandle_t q_actuator;
  EventGroupHandle_t event_group;
  EventBits_t hb_mask;
  EventBits_t hb_bit;
  uint16_t miss_timeout_ms;
  uint16_t loop_delay_ms;
  void (*wdt_feed)(void);
} app_task_diag_cfg_t;

void app_task_diag_init(const app_task_diag_cfg_t *cfg);
void diag_task(void *pvParameters);

#endif
