#include "ui_idle_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include <string.h>
#include <stdio.h>

#define UI_DISP_W          122
#define UI_CHAR_W          8
#define UI_CHAR_H          16
#define UI_MAX_CHARS       15

typedef struct
{
  machine_state_t state;
  bool dmx_online;
  uint8_t pressure_pct;
  uint32_t fault_mask;
  uint16_t dmx_address;
  bool valid;
} ui_idle_data_t;

static sl_Page s_idle_page;
static ui_idle_data_t s_data;
static bool s_enter_menu;
static uint8_t s_pressure_display;
static uint8_t s_pressure_ring[4];
static uint8_t s_pressure_ring_idx;
static bool s_pressure_filter_inited;

static const char *uidle_state_text(machine_state_t state)
{
  switch(state)
  {
    case MACHINE_READY:
      return "RDY";
    case MACHINE_FIRING:
      return "FIR";
    case MACHINE_RELIEF:
      return "REL";
    case MACHINE_FAULT:
      return "FLT";
    case MACHINE_LOCKED:
      return "LCK";
    case MACHINE_SELFTEST:
      return "CHK";
    case MACHINE_BOOT:
    default:
      return "BOT";
  }
}

static void uidle_append_fault_str(char *buf, uint16_t buf_size, uint32_t fault_mask)
{
  const char *sep = "";

  buf[0] = '\0';
  if((fault_mask & 0x01U) != 0U) { strcat(buf, "E1"); sep = " "; }
  if((fault_mask & 0x02U) != 0U) { strcat(buf, sep); strcat(buf, "E2"); sep = " "; }
  if((fault_mask & 0x04U) != 0U) { strcat(buf, sep); strcat(buf, "E3"); sep = " "; }
  if((fault_mask & 0x08U) != 0U) { strcat(buf, sep); strcat(buf, "E4"); sep = " "; }
  if((fault_mask & 0x10U) != 0U) { strcat(buf, sep); strcat(buf, "E5"); sep = " "; }
  (void)buf_size;
}

static int uidle_center_x(const char *text)
{
  int len = (int)strlen(text);
  int w = len * UI_CHAR_W;
  if(w >= UI_DISP_W) return 0;
  return (UI_DISP_W - w) / 2;
}

static void idle_init(sl_Page *self)
{
  (void)self;
  s_data.state = MACHINE_BOOT;
  s_data.dmx_online = false;
  s_data.pressure_pct = 0U;
  s_data.fault_mask = 0U;
  s_data.valid = false;
  s_enter_menu = false;
  s_pressure_filter_inited = false;
}

static void idle_draw(sl_Page *self)
{
  char row1[UI_MAX_CHARS + 1];
  char row2[UI_MAX_CHARS + 1];
  int x;

  (void)self;

  snprintf(row1, sizeof(row1), "%s D:%03u P:%u",
           uidle_state_text(s_data.state),
           (unsigned)s_data.dmx_address,
           (unsigned)s_data.pressure_pct);

  if(s_data.fault_mask != 0U)
  {
    uidle_append_fault_str(row2, sizeof(row2), s_data.fault_mask);
  }
  else
  {
    strcpy(row2, SL_LANG(SL_STR_PRESS_MENU));
  }

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  x = uidle_center_x(row1);
  sl_disp_draw_string((uint16_t)x, 0, row1, &sl_font_chinese, 0);

  x = uidle_center_x(row2);
  sl_disp_draw_string((uint16_t)x, 16, row2, &sl_font_chinese, 0);
}

static int idle_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  if(evt->type == SL_EVT_KEY_BACK)
  {
    s_enter_menu = true;
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
    s_idle_page.data = 0;
    s_idle_page.arg = 0;
  }
  return &s_idle_page;
}

bool ui_idle_page_consume_enter_menu(void)
{
  bool v = s_enter_menu;
  s_enter_menu = false;
  return v;
}

void ui_idle_page_update(machine_state_t state, bool dmx_online, uint8_t pressure_pct, uint32_t fault_mask, uint16_t dmx_address)
{
  bool changed;
  uint8_t filtered_pct;

  if(s_pressure_filter_inited == false)
  {
    uint8_t i;
    for(i = 0U; i < 4U; i++)
    {
      s_pressure_ring[i] = pressure_pct;
    }
    s_pressure_ring_idx = 0U;
    s_pressure_display = pressure_pct;
    s_pressure_filter_inited = true;
    filtered_pct = pressure_pct;
  }
  else
  {
    uint16_t sum;
    uint8_t avg;

    s_pressure_ring[s_pressure_ring_idx] = pressure_pct;
    s_pressure_ring_idx = (uint8_t)((s_pressure_ring_idx + 1U) & 0x03U);

    sum = (uint16_t)s_pressure_ring[0] + (uint16_t)s_pressure_ring[1]
        + (uint16_t)s_pressure_ring[2] + (uint16_t)s_pressure_ring[3];
    avg = (uint8_t)((sum + 2U) / 4U);

    if(avg != s_pressure_display)
    {
      s_pressure_display = avg;
    }
    filtered_pct = s_pressure_display;
  }

  changed = (s_data.state != state) ||
            (s_data.dmx_online != dmx_online) ||
            (s_data.pressure_pct != filtered_pct) ||
            (s_data.fault_mask != fault_mask) ||
            (s_data.dmx_address != dmx_address);

  s_data.state = state;
  s_data.dmx_online = dmx_online;
  s_data.pressure_pct = filtered_pct;
  s_data.fault_mask = fault_mask;
  s_data.dmx_address = dmx_address;
  s_data.valid = true;

  if(changed)
  {
    sl_page_request_redraw();
  }
}
