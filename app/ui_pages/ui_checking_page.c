#include "ui_checking_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>
#include <stdio.h>

#define UI_DISP_W          122
#define UI_CHECKING_MS     5000U
#define UI_CHECKING_PRESSURE_UPDATE_MS 200U
#define UI_DOTS_INTERVAL_MS 300U
#define UI_CHECKING_FONT_H          16U

static sl_Page s_check_page;

typedef struct
{
  machine_state_t state;
  uint8_t pressure_pct;
  uint32_t fault_mask;
  uint32_t pressure_update_tick;
  uint32_t enter_tick_ms;
  uint16_t base_text_w;
  uint8_t last_dots;
  uint8_t last_pressure_pct;
  uint32_t last_fault_mask;
  machine_state_t last_state;
  bool navigated;
} ui_checking_state_t;

static ui_checking_state_t s_check_state;

static ui_checking_state_t *uicp_state(sl_Page *self)
{
  return (ui_checking_state_t *)self->data;
}

static void uicp_init(sl_Page *self)
{
  ui_checking_state_t *st = uicp_state(self);
  st->state = MACHINE_SELFTEST;
  st->pressure_pct = 0U;
  st->fault_mask = 0U;
  st->pressure_update_tick = 0U;
  st->enter_tick_ms = sl_ui_get_tick();
  st->base_text_w = sl_text_measure_width(SL_LANG(SL_STR_CHECKING), &sl_font_chinese);
  st->last_dots = 0xFFU;
  st->last_pressure_pct = 0xFFU;
  st->last_fault_mask = 0xFFFFFFFFUL;
  st->last_state = (machine_state_t)0xFF;
  st->navigated = false;
}

static void uicp_sync_from_service(sl_Page *self)
{
  ui_checking_state_t *st = uicp_state(self);
  ui_machine_snapshot_t snap;
  uint32_t now_tick;

  if(ui_service_get_machine_snapshot(&snap) == false)
  {
    return;
  }

  st->state = snap.state;
  st->fault_mask = snap.fault_mask;

  now_tick = sl_ui_get_tick();
  if((st->pressure_update_tick == 0U) ||
     ((now_tick - st->pressure_update_tick) >= UI_CHECKING_PRESSURE_UPDATE_MS))
  {
    st->pressure_pct = snap.pressure_pct;
    st->pressure_update_tick = now_tick;
  }
}

static void uicp_draw(sl_Page *self)
{
  ui_checking_state_t *st = uicp_state(self);
  char row2[32];
  const char *base;
  uint16_t base_x;
  uint16_t dot_x;
  uint8_t i;
  int dots;
  uint32_t lived_ms;
  int step;
  uicp_sync_from_service(self);

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  base = SL_LANG(SL_STR_CHECKING);
  lived_ms = sl_ui_get_tick() - st->enter_tick_ms;
  step = (int)(lived_ms / UI_DOTS_INTERVAL_MS);
  dots = step % 4;

  if((st->fault_mask != 0U) || (st->state == MACHINE_FAULT) || (st->state == MACHINE_LOCKED))
  {
    if((st->fault_mask & 0x01U) != 0U) { strcpy(row2, "E1"); }
    else if((st->fault_mask & 0x02U) != 0U) { strcpy(row2, "E2"); }
    else if((st->fault_mask & 0x04U) != 0U) { strcpy(row2, "E3"); }
    else if((st->fault_mask & 0x08U) != 0U) { strcpy(row2, "E4"); }
    else if((st->fault_mask & 0x10U) != 0U) { strcpy(row2, "E5"); }
    else { strcpy(row2, "FAULT"); }
  }
  else
  {
    snprintf(row2, sizeof(row2), "P:%3u%%", (unsigned)st->pressure_pct);
  }

  base_x = (uint16_t)((UI_DISP_W > st->base_text_w) ? ((UI_DISP_W - st->base_text_w) / 2U) : 0U);
  sl_disp_draw_string(base_x, 0, base, &sl_font_chinese, 0);

  dot_x = (uint16_t)(base_x + st->base_text_w);
  for(i = 0U; i < (uint8_t)dots; i++)
  {
    sl_disp_draw_string((uint16_t)(dot_x + (uint16_t)(i * 8U)), 0, ".", &sl_font_chinese, 0);
  }

  (void)sl_text_draw_center(UI_CHECKING_FONT_H, row2, &sl_font_chinese, 0);
}

static int uicp_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  (void)evt;
  return 0;
}

static void uicp_tick(sl_Page *self, uint16_t elapsed_ms)
{
  ui_checking_state_t *st = uicp_state(self);
  uint32_t lived_ms;
  uint8_t dots;
  uint8_t prev_pressure;
  uint32_t prev_fault_mask;
  machine_state_t prev_state;
  (void)elapsed_ms;

  prev_pressure = st->pressure_pct;
  prev_fault_mask = st->fault_mask;
  prev_state = st->state;
  uicp_sync_from_service(self);
  lived_ms = sl_ui_get_tick() - st->enter_tick_ms;
  dots = (uint8_t)((lived_ms / UI_DOTS_INTERVAL_MS) % 4UL);

  if(dots != st->last_dots)
  {
    st->last_dots = dots;
    sl_ui_request_redraw();
  }

  if((prev_pressure != st->pressure_pct) ||
     (prev_fault_mask != st->fault_mask) ||
     (prev_state != st->state))
  {
    st->last_pressure_pct = st->pressure_pct;
    st->last_fault_mask = st->fault_mask;
    st->last_state = st->state;
    sl_ui_request_redraw();
  }

  if((st->navigated == false) && (lived_ms >= UI_CHECKING_MS) && (st->state != MACHINE_SELFTEST))
  {
    st->navigated = true;
    APP_LOGI("ui page: checking -> idle (%lums)", lived_ms);
    (void)sl_ui_navigate("idle");
  }
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
    s_check_page.tick = uicp_tick;
    s_check_page.data = &s_check_state;
    s_check_page.arg = 0;
  }
  return &s_check_page;
}
