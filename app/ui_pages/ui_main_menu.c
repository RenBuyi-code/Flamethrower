#include "ui_main_menu.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include "../../app/log_rtt.h"
#include <string.h>

#define UI_DISP_W          122
#define UI_CHAR_W          8
#define UI_CHAR_H          16
#define UI_ARROW_X         0
#define UI_INDEX_X         8
#define UI_LABEL_X         24

static const uint8_t s_item_ids[UI_MENU_ITEM_COUNT] = {
    SL_STR_DMX_SET,
    SL_STR_PRESSURE_SET,
    SL_STR_TILT,
    SL_STR_LANGUAGE
};

static const char *s_item_names[UI_MENU_ITEM_COUNT] = {
    "DMX",
    "Delay",
    "Tilt",
    "Language"
};

static const char *s_item_targets[UI_MENU_ITEM_COUNT] = {
    "dmx_set",
    "delay_set",
    "safety",
    "language"
};

static sl_Page s_menu_page;

typedef struct
{
  int cursor;
  int scroll_off;
} ui_main_menu_state_t;

static ui_main_menu_state_t s_menu_state;

static ui_main_menu_state_t *uimm_state(sl_Page *self)
{
  return (ui_main_menu_state_t *)self->data;
}

static void uimm_init(sl_Page *self)
{
  ui_main_menu_state_t *st = uimm_state(self);
  st->cursor = 0;
  st->scroll_off = 0;
}

static void uimm_draw(sl_Page *self)
{
  int row0_idx, row1_idx;
  int i;
  ui_main_menu_state_t *st = uimm_state(self);

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  row0_idx = st->scroll_off;
  row1_idx = st->scroll_off + 1;

  for(i = 0; i < 2; i++)
  {
    int idx = (i == 0) ? row0_idx : row1_idx;
    int y = i * UI_CHAR_H;
    bool is_focused = (idx == st->cursor);
    char idx_buf[2];
    sl_TextSegment segs[3];
    uint8_t color = is_focused ? 1U : 0U;

    if(idx >= UI_MENU_ITEM_COUNT) break;

    idx_buf[0] = '1' + (char)idx;
    idx_buf[1] = '\0';

    if(is_focused)
    {
      sl_disp_fill_rect(0, y, SL_DISP_WIDTH, UI_CHAR_H, 0);
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
    segs[2].text = SL_LANG(s_item_ids[idx]);
    segs[2].font = &sl_font_chinese;
    sl_text_draw_segments((uint16_t)y, segs, 3U, color);
  }
}

static int uimm_proc(sl_Page *self, const sl_Event *evt)
{
  ui_main_menu_state_t *st = uimm_state(self);

  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
      if(st->cursor > 0)
      {
        st->cursor--;
        st->scroll_off = (st->cursor <= 1) ? 0 : (UI_MENU_ITEM_COUNT - 2);
        APP_LOGI("menu: cursor=%d (%s)", st->cursor, s_item_names[st->cursor]);
        sl_ui_request_redraw();
      }
      break;

    case SL_EVT_KEY_DOWN:
      if(st->cursor < UI_MENU_ITEM_COUNT - 1)
      {
        st->cursor++;
        st->scroll_off = (st->cursor <= 1) ? 0 : (UI_MENU_ITEM_COUNT - 2);
        APP_LOGI("menu: cursor=%d (%s)", st->cursor, s_item_names[st->cursor]);
        sl_ui_request_redraw();
      }
      break;

    case SL_EVT_KEY_ENTER:
      APP_LOGI("menu: select=%d (%s)", st->cursor, s_item_names[st->cursor]);
      (void)sl_ui_navigate(s_item_targets[st->cursor]);
      sl_ui_request_redraw();
      break;

    case SL_EVT_KEY_BACK:
      return 1;

    default:
      break;
  }
  return 0;
}

sl_Page *ui_main_menu_get(void)
{
  if(s_menu_page.init == 0)
  {
    s_menu_page.name = "main_menu";
    s_menu_page.init = uimm_init;
    s_menu_page.draw = uimm_draw;
    s_menu_page.proc = uimm_proc;
    s_menu_page.exit = 0;
    s_menu_page.presenter = 0;
    s_menu_page.tick = 0;
    s_menu_page.data = &s_menu_state;
    s_menu_page.arg = 0;
  }
  return &s_menu_page;
}
