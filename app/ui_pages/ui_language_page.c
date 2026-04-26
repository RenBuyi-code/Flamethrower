#include "ui_language_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>
#include <stdint.h>

#define UI_CHAR_H          16
#define UI_ARROW_X         0
#define UI_INDEX_X         8
#define UI_LABEL_X         24

static sl_Page s_lang_page;

typedef struct
{
  int cursor;
} ui_language_state_t;

static ui_language_state_t s_lang_state;

static ui_language_state_t *uilp_state(sl_Page *self)
{
  return (ui_language_state_t *)self->data;
}

static void uilp_init(sl_Page *self)
{
  ui_language_state_t *st = uilp_state(self);
  st->cursor = (sl_lang_get_current() == SL_LANG_CN) ? 0 : 1;
  APP_LOGI("ui language: init cursor=%d", st->cursor);
}

static void uilp_draw(sl_Page *self)
{
  char idx_buf[2];
  ui_language_state_t *st = uilp_state(self);
  sl_TextSegment segs[3];

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  idx_buf[0] = '1';
  idx_buf[1] = '\0';

  if(st->cursor == 0)
  {
    sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, UI_CHAR_H, 0);
    segs[0].text = ">";
  }
  else
  {
    segs[0].text = " ";
  }
  segs[0].x = UI_ARROW_X;
  segs[0].font = &sl_default_font;
  segs[1].x = UI_INDEX_X;
  segs[1].text = idx_buf;
  segs[1].font = &sl_default_font;
  segs[2].x = UI_LABEL_X;
  segs[2].text = SL_LANG(SL_STR_CN);
  segs[2].font = &sl_font_chinese;
  sl_text_draw_segments(0, segs, 3U, (st->cursor == 0) ? 1U : 0U);

  idx_buf[0] = '2';
  idx_buf[1] = '\0';

  if(st->cursor == 1)
  {
    sl_disp_fill_rect(0, 16, SL_DISP_WIDTH, UI_CHAR_H, 0);
    segs[0].text = ">";
  }
  else
  {
    segs[0].text = " ";
  }
  segs[0].x = UI_ARROW_X;
  segs[0].font = &sl_default_font;
  segs[1].x = UI_INDEX_X;
  segs[1].text = idx_buf;
  segs[1].font = &sl_default_font;
  segs[2].x = UI_LABEL_X;
  segs[2].text = SL_LANG(SL_STR_EN);
  segs[2].font = &sl_font_chinese;
  sl_text_draw_segments(16, segs, 3U, (st->cursor == 1) ? 1U : 0U);
}

static int uilp_proc(sl_Page *self, const sl_Event *evt)
{
  ui_language_state_t *st = uilp_state(self);

  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
      st->cursor = (st->cursor == 0) ? 1 : 0;
      APP_LOGI("ui language: key up cursor=%d", st->cursor);
      sl_ui_request_redraw();
      break;

    case SL_EVT_KEY_DOWN:
      st->cursor = (st->cursor == 1) ? 0 : 1;
      APP_LOGI("ui language: key down cursor=%d", st->cursor);
      sl_ui_request_redraw();
      break;

    case SL_EVT_KEY_ENTER:
    {
      int lang = (st->cursor == 0) ? SL_LANG_CN : SL_LANG_EN;
      APP_LOGI("ui language: key enter cursor=%d", st->cursor);
      sl_lang_set(lang);
      ui_service_save_language((int16_t)lang);
      sl_ui_go_back();
      break;
    }

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
  if(s_lang_page.init == 0)
  {
    s_lang_page.name = "language";
    s_lang_page.init = uilp_init;
    s_lang_page.draw = uilp_draw;
    s_lang_page.proc = uilp_proc;
    s_lang_page.exit = 0;
    s_lang_page.presenter = 0;
    s_lang_page.tick = 0;
    s_lang_page.data = &s_lang_state;
    s_lang_page.arg = 0;
    APP_LOGI("language: page init set, addr=%p, init=%p", (void*)&s_lang_page, (void*)(uintptr_t)s_lang_page.init);
  }
  APP_LOGI("language: get page, addr=%p, init=%p", (void*)&s_lang_page, (void*)(uintptr_t)s_lang_page.init);
  return &s_lang_page;
}
