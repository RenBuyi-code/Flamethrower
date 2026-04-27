#include "task_diag.h"
#include "../app_task_common.h"
#include "../app_task_shared.h"

static app_task_diag_cfg_t s_task_diag_cfg;

void app_task_diag_init(const app_task_diag_cfg_t *cfg)
{
  if(cfg == 0)
  {
    s_task_diag_cfg.q_actuator = 0;
    s_task_diag_cfg.event_group = 0;
    s_task_diag_cfg.hb_mask = 0U;
    s_task_diag_cfg.hb_bit = 0U;
    s_task_diag_cfg.miss_timeout_ms = 0U;
    s_task_diag_cfg.loop_delay_ms = 0U;
    s_task_diag_cfg.wdt_feed = 0;
    return;
  }

  s_task_diag_cfg = *cfg;
}

void diag_task(void *pvParameters)
{
  EventBits_t bits;
  TickType_t miss_since;
  actuator_cmd_t cmd;
  bool hb_ok;
  (void)pvParameters;

  miss_since = 0U;

  for(;;)
  {
    if((s_task_diag_cfg.q_actuator == 0) ||
       (s_task_diag_cfg.event_group == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    bits = xEventGroupGetBits(s_task_diag_cfg.event_group);
    hb_ok = ((bits & s_task_diag_cfg.hb_mask) == s_task_diag_cfg.hb_mask);
    if(hb_ok == false)
    {
      if(miss_since == 0U)
      {
        miss_since = xTaskGetTickCount();
      }
      else if((xTaskGetTickCount() - miss_since) >= pdMS_TO_TICKS(s_task_diag_cfg.miss_timeout_ms))
      {
        cmd.type = ACT_CMD_SAFE_OFF;
        cmd.priority = 10U;
        cmd.user_mode = false;
        cmd.igniter_delay_ms = 0U;
        cmd.oil_lock_delay_ms = 0U;
        cmd.fire_duration_ms = 0U;
        app_task_queue_send_latest(s_task_diag_cfg.q_actuator, &cmd, pdTRUE);
      }
    }
    else
    {
      miss_since = 0U;
      if(s_task_diag_cfg.wdt_feed != 0)
      {
        s_task_diag_cfg.wdt_feed();
      }
    }

    (void)xEventGroupClearBits(s_task_diag_cfg.event_group, s_task_diag_cfg.hb_mask);
    (void)xEventGroupSetBits(s_task_diag_cfg.event_group, s_task_diag_cfg.hb_bit);
    vTaskDelay(pdMS_TO_TICKS(s_task_diag_cfg.loop_delay_ms));
  }
}
