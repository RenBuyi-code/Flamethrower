#include "ui_safety_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>
#include <stdint.h>

#define UI_CHAR_H          16

static sl_Page s_safety_page;
typedef struct
{
  int16_t *tilt_ref;
} ui_safety_state_t;

static ui_safety_state_t s_safety_state;

static ui_safety_state_t *uisf_state(sl_Page *self)
{
  return SL_PAGE_DATA_AS(ui_safety_state_t, self);
}

static void uisf_init(sl_Page *self)
{
  (void)self;
  APP_LOGI("safety: init (tilt page)");
}

static void uisf_draw(sl_Page *self)
{
  ui_safety_state_t *st = uisf_state(self);
  char row1[20];
  char row2[20];
  const char *state_text;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  state_text = ((st->tilt_ref != 0) && (*st->tilt_ref != 0)) ? SL_LANG(SL_STR_ON) : SL_LANG(SL_STR_OFF);
  row1[0] = '>';
  row1[1] = '\0';
  strcat(row1, SL_LANG(SL_STR_TILT));
  strcat(row1, ":");
  strcat(row1, state_text);
  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, UI_CHAR_H, 0);
  sl_disp_draw_string(0, 0, row1, &sl_font_chinese, 1);

  strcpy(row2, SL_LANG(SL_STR_PRESS_MENU));
  sl_disp_draw_string(0, 16, row2, &sl_font_chinese, 0);
}

static int uisf_proc(sl_Page *self, const sl_Event *evt)
{
  ui_safety_state_t *st = uisf_state(self);
  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
    case SL_EVT_KEY_DOWN:
    case SL_EVT_KEY_ENTER:
      if(st->tilt_ref != 0)
      {
        *st->tilt_ref = (*st->tilt_ref == 0) ? 1 : 0;
        ui_service_save_tilt_enable(*st->tilt_ref);
        sl_ui_request_redraw();
      }
      break;
    case SL_EVT_KEY_BACK:
      return 1;
    default:
      break;
  }
  return 0;
}

sl_Page *ui_safety_page_get(void)
{
  if(s_safety_page.init == 0)
  {
    s_safety_page.name = "safety";
    s_safety_page.init = uisf_init;
    s_safety_page.draw = uisf_draw;
    s_safety_page.proc = uisf_proc;
    s_safety_page.exit = 0;
    s_safety_page.presenter = 0;
    s_safety_page.tick = 0;
    s_safety_page.data = &s_safety_state;
    s_safety_page.arg = 0;
    APP_LOGI("safety: page init set, addr=%p, init=%p", (void*)&s_safety_page, (void*)(uintptr_t)s_safety_page.init);
  }
  APP_LOGI("safety: get page, addr=%p, init=%p", (void*)&s_safety_page, (void*)(uintptr_t)s_safety_page.init);
  return &s_safety_page;
}

void ui_safety_page_set_tilt_ref(int16_t *tilt_enable)
{
  s_safety_state.tilt_ref = tilt_enable;
}
