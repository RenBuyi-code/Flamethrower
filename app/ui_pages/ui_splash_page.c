#include "ui_splash_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>

#define UI_DISP_W          122
#define UI_DISP_H          32
#define UI_SPLASH_MS       2000U
#define UI_FONT_H          16

static sl_Page s_splash_page;

typedef struct
{
  uint32_t start_tick_ms;
  bool navigated;
} ui_splash_state_t;

static ui_splash_state_t s_splash_state;

static ui_splash_state_t *uisp_state(sl_Page *self)
{
  return (ui_splash_state_t *)self->data;
}

static void uisp_init(sl_Page *self)
{
  ui_splash_state_t *st = uisp_state(self);
  st->start_tick_ms = sl_ui_get_tick();
  st->navigated = false;
}

static int uisp_center_y(void)
{
  if(UI_FONT_H >= UI_DISP_H) return 0;
  return (UI_DISP_H - UI_FONT_H) / 2;
}

static void uisp_draw(sl_Page *self)
{
  const char *msg;
  int x;
  int y;
  (void)self;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  msg = SL_LANG(SL_STR_WELCOME);
  y = uisp_center_y();
  x = (int)sl_text_draw_center((uint16_t)y, msg, &sl_font_chinese, 0);
  (void)x;
}

static int uisp_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  (void)evt;
  return 0;
}

static void uisp_tick(sl_Page *self, uint16_t elapsed_ms)
{
  ui_splash_state_t *st = uisp_state(self);
  uint32_t lived_ms;
  (void)elapsed_ms;

  if(st->navigated)
  {
    return;
  }

  lived_ms = sl_ui_get_tick() - st->start_tick_ms;
  if(lived_ms >= UI_SPLASH_MS)
  {
    st->navigated = true;
    APP_LOGI("ui page: splash -> checking (%lums)", lived_ms);
    (void)sl_ui_navigate("checking");
  }
}

sl_Page *ui_splash_page_get(void)
{
  if(s_splash_page.init == 0)
  {
    s_splash_page.name = "splash";
    s_splash_page.init = uisp_init;
    s_splash_page.draw = uisp_draw;
    s_splash_page.proc = uisp_proc;
    s_splash_page.exit = 0;
    s_splash_page.presenter = 0;
    s_splash_page.tick = uisp_tick;
    s_splash_page.data = &s_splash_state;
    s_splash_page.arg = 0;
  }
  return &s_splash_page;
}
