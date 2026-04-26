#include "ui_splash_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define UI_DISP_W          122
#define UI_DISP_H          32
#define UI_SPLASH_MS       2000U
#define UI_FONT_H          16

static sl_Page s_splash_page;
static uint32_t s_start_tick_ms;

static void uisp_init(sl_Page *self)
{
  (void)self;
  s_start_tick_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static int uisp_center_x(const char *text)
{
  const unsigned char *p = (const unsigned char *)text;
  int w = 0;

  while(*p != '\0')
  {
    if(*p < 0x80U)
    {
      w += 8;
      p += 1;
    }
    else if(((*p & 0xF0U) == 0xE0U) && (p[1] != '\0') && (p[2] != '\0'))
    {
      w += 16;
      p += 3;
    }
    else if(((*p & 0xE0U) == 0xC0U) && (p[1] != '\0'))
    {
      w += 16;
      p += 2;
    }
    else
    {
      w += 16;
      p += 1;
    }
  }

  if(w >= UI_DISP_W) return 0;
  return (UI_DISP_W - w) / 2;
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
  x = uisp_center_x(msg);
  y = uisp_center_y();
  sl_disp_draw_string((uint16_t)x, (uint16_t)y, msg, &sl_font_chinese, 0);
}

static int uisp_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;
  (void)evt;
  return 1;
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
    s_splash_page.data = 0;
    s_splash_page.arg = 0;
  }
  return &s_splash_page;
}

void ui_splash_page_tick(void)
{
  sl_page_request_redraw();
}

bool ui_splash_page_is_done(void)
{
  uint32_t now_ms;

  now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
  return ((now_ms - s_start_tick_ms) >= UI_SPLASH_MS);
}
