#include "task_dmx.h"

static app_task_dmx_cfg_t s_task_dmx_cfg;

void app_task_dmx_init(const app_task_dmx_cfg_t *cfg)
{
  if(cfg == 0)
  {
    s_task_dmx_cfg.app = 0;
    s_task_dmx_cfg.rx = 0;
    s_task_dmx_cfg.event_group = 0;
    s_task_dmx_cfg.hb_bit = 0U;
    return;
  }

  s_task_dmx_cfg = *cfg;
}

void dmx_task(void *pvParameters)
{
  edmx_event_t evt;
  bool is_break;
  uint8_t b;
  (void)pvParameters;

  for(;;)
  {
    if((s_task_dmx_cfg.app == 0) || (s_task_dmx_cfg.rx == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    while(s_task_dmx_cfg.app->hal.dmx.poll_byte(s_task_dmx_cfg.app->hal.dmx.ctx, &b, &is_break))
    {
      evt.byte = b;
      evt.flags = is_break ? EDMX_EVENT_FLAG_BREAK : 0U;
      (void)edmx_rx_push_event(s_task_dmx_cfg.rx, &evt);
    }

    if(s_task_dmx_cfg.event_group != 0)
    {
      (void)xEventGroupSetBits(s_task_dmx_cfg.event_group, s_task_dmx_cfg.hb_bit);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
