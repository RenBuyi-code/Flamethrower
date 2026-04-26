#include "ui_idle_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define UI_PRESSURE_UPDATE_MS 500U

typedef struct
{
  machine_state_t state;
  bool dmx_online;
  bool pumping;
  uint8_t pressure_pct;
  uint32_t fault_mask;
  uint16_t dmx_address;
  bool valid;
} ui_idle_data_t;

typedef struct
{
  ui_idle_data_t data;
  uint8_t pressure_display;
  uint8_t pressure_ring[4];
  uint8_t pressure_ring_idx;
  bool pressure_filter_inited;
  TickType_t pressure_update_tick;
} ui_idle_state_t;

static sl_Page s_idle_page;
static ui_idle_state_t s_idle_state;

static ui_idle_state_t *uidle_state(sl_Page *self)
{
  return SL_PAGE_DATA_AS(ui_idle_state_t, self);
}

static const char *uidle_fault_text(uint32_t fault_mask)
{
  if((fault_mask & 0x01U) != 0U)
  {
    return SL_LANG(SL_STR_E1_PRESSURE);
  }
  if((fault_mask & 0x02U) != 0U)
  {
    return SL_LANG(SL_STR_E2_TILT_FAULT);
  }
  if((fault_mask & 0x04U) != 0U)
  {
    return SL_LANG(SL_STR_E3_VOLTAGE);
  }
  if((fault_mask & 0x08U) != 0U)
  {
    return SL_LANG(SL_STR_E4_LOCKED_FAULT);
  }
  if((fault_mask & 0x10U) != 0U)
  {
    return SL_LANG(SL_STR_E5_RELIEF);
  }
  return SL_LANG(SL_STR_FLT);
}

static void idle_init(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);

  memset(st, 0, sizeof(*st));
  st->data.state = MACHINE_BOOT;
}

static void idle_sync_from_service(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);
  ui_machine_snapshot_t snap;
  uint8_t filtered_pct;
  TickType_t now_tick;

  if(ui_service_get_machine_snapshot(&snap) == false)
  {
    return;
  }

  if(st->pressure_filter_inited == false)
  {
    uint8_t i;

    for(i = 0U; i < 4U; i++)
    {
      st->pressure_ring[i] = snap.pressure_pct;
    }

    st->pressure_ring_idx = 0U;
    st->pressure_display = snap.pressure_pct;
    st->pressure_filter_inited = true;
    filtered_pct = snap.pressure_pct;
  }
  else
  {
    uint16_t sum;
    uint8_t avg;

    st->pressure_ring[st->pressure_ring_idx] = snap.pressure_pct;
    st->pressure_ring_idx = (uint8_t)((st->pressure_ring_idx + 1U) & 0x03U);

    sum = (uint16_t)st->pressure_ring[0] + (uint16_t)st->pressure_ring[1]
        + (uint16_t)st->pressure_ring[2] + (uint16_t)st->pressure_ring[3];
    avg = (uint8_t)((sum + 2U) / 4U);
    st->pressure_display = avg;
    filtered_pct = avg;
  }

  now_tick = xTaskGetTickCount();
  if((st->pressure_update_tick == 0U) ||
     ((now_tick - st->pressure_update_tick) >= pdMS_TO_TICKS(UI_PRESSURE_UPDATE_MS)))
  {
    st->data.pressure_pct = filtered_pct;
    st->pressure_update_tick = now_tick;
  }

  st->data.state = snap.state;
  st->data.dmx_online = snap.dmx_online;
  st->data.pumping = snap.pumping;
  st->data.fault_mask = snap.fault_mask;
  st->data.dmx_address = snap.dmx_addr;
  st->data.valid = true;
}

static void idle_draw(sl_Page *self)
{
  ui_idle_state_t *st = uidle_state(self);
  char row1[16];
  const char *row2;

  idle_sync_from_service(self);

  snprintf(row1, sizeof(row1), "DMX:%03u", (unsigned)st->data.dmx_address);

  if((st->data.fault_mask != 0U) ||
     (st->data.state == MACHINE_FAULT) ||
     (st->data.state == MACHINE_LOCKED))
  {
    row2 = uidle_fault_text(st->data.fault_mask);
  }
  else
  {
    row2 = (st->data.pumping == false) ? SL_LANG(SL_STR_READY) : SL_LANG(SL_STR_CHARGING);
  }

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);
  (void)sl_text_draw_center(0, row1, &sl_font_chinese, 0);
  (void)sl_text_draw_center(16, row2, &sl_font_chinese, 0);
}

static int idle_proc(sl_Page *self, const sl_Event *evt)
{
  ui_idle_state_t *st = uidle_state(self);

  if((st->data.fault_mask != 0U) ||
     (st->data.state == MACHINE_FAULT) ||
     (st->data.state == MACHINE_LOCKED))
  {
    return 0;
  }

  if(evt->type == SL_EVT_KEY_BACK)
  {
    (void)sl_ui_navigate("main_menu");
  }

  return 0;
}

sl_Page *ui_idle_page_get(void)
{
  if(s_idle_page.init == 0)
  {
    s_idle_page.name = "idle";
    s_idle_page.init = idle_init;
    s_idle_page.draw = idle_draw;
    s_idle_page.proc = idle_proc;
    s_idle_page.exit = 0;
    s_idle_page.presenter = 0;
    s_idle_page.tick = 0;
    s_idle_page.data = &s_idle_state;
    s_idle_page.arg = 0;
  }

  return &s_idle_page;
}
