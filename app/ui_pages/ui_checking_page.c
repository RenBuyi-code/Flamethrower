#include "ui_checking_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#define UI_DISP_W          122
#define UI_PRESSURE_UPDATE_MS 500U

static sl_Page s_check_page;
static machine_state_t s_state;
static uint8_t s_pressure_pct;
static uint32_t s_fault_mask;
static TickType_t s_pressure_update_tick;

static void uicp_init(sl_Page *self)
{
  (void)self;
  s_state = MACHINE_SELFTEST;
  s_pressure_pct = 0U;
  s_fault_mask = 0U;
  s_pressure_update_tick = 0U;
}

static int uicp_center_x(const char *text)
{
  int len = (int)strlen(text);
  int w = len * 16;
  if(w >= UI_DISP_W) return 0;
  return (UI_DISP_W - w) / 2;
}

static void uicp_draw(sl_Page *self)
{
  char row1[32];
  char row2[32];
  const char *base;
  int x;
  int dots;
  uint32_t elapsed_ms;
  int step;
  (void)self;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  base = SL_LANG(SL_STR_CHECKING);
  elapsed_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
  step = (int)(elapsed_ms / 300UL);
  dots = step % 4;

  strcpy(row1, base);

  {
    int i;
    int len = (int)strlen(row1);
    for(i = 0; i < dots; i++)
    {
      row1[len + i] = '.';
    }
    row1[len + dots] = '\0';
  }

  if((s_fault_mask != 0U) || (s_state == MACHINE_FAULT) || (s_state == MACHINE_LOCKED))
  {
    if((s_fault_mask & 0x01U) != 0U) { strcpy(row2, "E1"); }
    else if((s_fault_mask & 0x02U) != 0U) { strcpy(row2, "E2"); }
    else if((s_fault_mask & 0x04U) != 0U) { strcpy(row2, "E3"); }
    else if((s_fault_mask & 0x08U) != 0U) { strcpy(row2, "E4"); }
    else if((s_fault_mask & 0x10U) != 0U) { strcpy(row2, "E5"); }
    else { strcpy(row2, "FAULT"); }
  }
  else
  {
    snprintf(row2, sizeof(row2), "P:%3u%%", (unsigned)s_pressure_pct);
  }

  x = uicp_center_x(row1);
  sl_disp_draw_string((uint16_t)x, 0, row1, &sl_font_chinese, 0);
  x = uicp_center_x(row2);
  sl_disp_draw_string((uint16_t)x, 16, row2, &sl_font_chinese, 0);
}

static int uicp_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  (void)evt;
  sl_page_request_redraw();
  return 1;
}

sl_Page *ui_checking_page_get(void)
{
  if(s_check_page.init == 0)
  {
    s_check_page.name = "checking";
    s_check_page.init = uicp_init;
    s_check_page.draw = uicp_draw;
    s_check_page.proc = uicp_proc;
    s_check_page.exit = 0;
    s_check_page.presenter = 0;
    s_check_page.data = 0;
    s_check_page.arg = 0;
  }
  return &s_check_page;
}

void ui_checking_page_update(machine_state_t state, uint8_t pressure_pct, uint32_t fault_mask)
{
  TickType_t now_tick;
  bool changed;

  now_tick = xTaskGetTickCount();
  changed = (s_state != state) || (s_fault_mask != fault_mask);

  s_state = state;
  s_fault_mask = fault_mask;

  if((s_pressure_update_tick == 0U) ||
     ((now_tick - s_pressure_update_tick) >= pdMS_TO_TICKS(UI_PRESSURE_UPDATE_MS)))
  {
    if(s_pressure_pct != pressure_pct)
    {
      changed = true;
      s_pressure_pct = pressure_pct;
    }
    s_pressure_update_tick = now_tick;
  }

  if(changed)
  {
    sl_page_request_redraw();
  }
}

bool ui_checking_page_is_done(void)
{
  return (s_state != MACHINE_SELFTEST);
}
