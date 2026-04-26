#include "ui_safety_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include <string.h>

#define UI_CHAR_H          16

static sl_Page s_page;
static int16_t *s_tilt_ref;
static bool s_tilt_changed;

static void uisf_init(sl_Page *self)
{
  (void)self;
  s_tilt_changed = false;
}

static void uisf_draw(sl_Page *self)
{
  char row1[20];
  char row2[20];
  const char *state_text;
  (void)self;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  state_text = ((s_tilt_ref != 0) && (*s_tilt_ref != 0)) ? SL_LANG(SL_STR_ON) : SL_LANG(SL_STR_OFF);
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
  (void)self;
  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
    case SL_EVT_KEY_DOWN:
    case SL_EVT_KEY_ENTER:
      if(s_tilt_ref != 0)
      {
        *s_tilt_ref = (*s_tilt_ref == 0) ? 1 : 0;
        s_tilt_changed = true;
        sl_page_request_redraw();
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
  if(s_page.init == 0)
  {
    s_page.name = "safety";
    s_page.init = uisf_init;
    s_page.draw = uisf_draw;
    s_page.proc = uisf_proc;
    s_page.exit = 0;
    s_page.presenter = 0;
    s_page.data = 0;
    s_page.arg = 0;
  }
  return &s_page;
}

void ui_safety_page_set_tilt_ref(int16_t *tilt_enable)
{
  s_tilt_ref = tilt_enable;
}

int ui_safety_page_consume_tilt_changed(void)
{
  if(s_tilt_changed == false)
  {
    return -1;
  }
  s_tilt_changed = false;
  if(s_tilt_ref == 0)
  {
    return -1;
  }
  return (*s_tilt_ref != 0) ? 1 : 0;
}
