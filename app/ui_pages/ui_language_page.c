#include "ui_language_page.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>

#define UI_CHAR_H          16
#define UI_ARROW_X         0
#define UI_INDEX_X         8
#define UI_LABEL_X         24

static sl_Page s_page;
static int s_lang_cursor;
static bool s_lang_selected;

static void uilp_init(sl_Page *self)
{
  (void)self;
  s_lang_cursor = (sl_lang_get_current() == SL_LANG_CN) ? 0 : 1;
  s_lang_selected = false;
  APP_LOGI("ui language: init cursor=%d", s_lang_cursor);
}

static void uilp_draw(sl_Page *self)
{
  char idx_buf[2];
  (void)self;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  idx_buf[0] = '1';
  idx_buf[1] = '\0';

  if(s_lang_cursor == 0)
  {
    sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, UI_CHAR_H, 0);
    sl_disp_draw_string(UI_ARROW_X, 0, ">", &sl_default_font, 1);
    sl_disp_draw_string(UI_INDEX_X, 0, idx_buf, &sl_default_font, 1);
    sl_disp_draw_string(UI_LABEL_X, 0, SL_LANG(SL_STR_CN), &sl_font_chinese, 1);
  }
  else
  {
    sl_disp_draw_string(UI_ARROW_X, 0, " ", &sl_default_font, 0);
    sl_disp_draw_string(UI_INDEX_X, 0, idx_buf, &sl_default_font, 0);
    sl_disp_draw_string(UI_LABEL_X, 0, SL_LANG(SL_STR_CN), &sl_font_chinese, 0);
  }

  idx_buf[0] = '2';
  idx_buf[1] = '\0';

  if(s_lang_cursor == 1)
  {
    sl_disp_fill_rect(0, 16, SL_DISP_WIDTH, UI_CHAR_H, 0);
    sl_disp_draw_string(UI_ARROW_X, 16, ">", &sl_default_font, 1);
    sl_disp_draw_string(UI_INDEX_X, 16, idx_buf, &sl_default_font, 1);
    sl_disp_draw_string(UI_LABEL_X, 16, SL_LANG(SL_STR_EN), &sl_font_chinese, 1);
  }
  else
  {
    sl_disp_draw_string(UI_ARROW_X, 16, " ", &sl_default_font, 0);
    sl_disp_draw_string(UI_INDEX_X, 16, idx_buf, &sl_default_font, 0);
    sl_disp_draw_string(UI_LABEL_X, 16, SL_LANG(SL_STR_EN), &sl_font_chinese, 0);
  }
}

static int uilp_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;

  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
      s_lang_cursor = (s_lang_cursor == 0) ? 1 : 0;
      APP_LOGI("ui language: key up cursor=%d", s_lang_cursor);
      sl_page_request_redraw();
      break;

    case SL_EVT_KEY_DOWN:
      s_lang_cursor = (s_lang_cursor == 1) ? 0 : 1;
      APP_LOGI("ui language: key down cursor=%d", s_lang_cursor);
      sl_page_request_redraw();
      break;

    case SL_EVT_KEY_ENTER:
      APP_LOGI("ui language: key enter cursor=%d", s_lang_cursor);
      s_lang_selected = true;
      return 1;

    case SL_EVT_KEY_BACK:
      APP_LOGI("ui language: key back");
      return 1;

    default:
      break;
  }
  return 0;
}

sl_Page *ui_language_page_get(void)
{
  if(s_page.init == 0)
  {
    s_page.name = "language";
    s_page.init = uilp_init;
    s_page.draw = uilp_draw;
    s_page.proc = uilp_proc;
    s_page.exit = 0;
    s_page.presenter = 0;
    s_page.data = 0;
    s_page.arg = 0;
  }
  return &s_page;
}

int ui_language_page_consume_selection(void)
{
  int lang;

  if(!s_lang_selected) return -1;
  s_lang_selected = false;

  lang = (s_lang_cursor == 0) ? SL_LANG_CN : SL_LANG_EN;
  sl_lang_set(lang);
  sl_page_request_redraw();
  return lang;
}
