#include "ui_main_menu.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_page_manager.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
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
    SL_STR_LANGUAGE
};

static sl_Page s_menu_page;
static int s_cursor;
static int s_scroll_off;
static int s_selected;
static bool s_back_to_idle;

static void uimm_init(sl_Page *self)
{
  (void)self;
  s_cursor = 0;
  s_scroll_off = 0;
  s_selected = UI_MENU_ITEM_NONE;
  s_back_to_idle = false;
}

static void uimm_draw(sl_Page *self)
{
  int row0_idx, row1_idx;
  int i;
  (void)self;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  row0_idx = s_scroll_off;
  row1_idx = s_scroll_off + 1;

  for(i = 0; i < 2; i++)
  {
    int idx = (i == 0) ? row0_idx : row1_idx;
    int y = i * UI_CHAR_H;
    bool is_focused = (idx == s_cursor);
    char idx_buf[2];
    uint8_t color = is_focused ? 1U : 0U;

    if(idx >= UI_MENU_ITEM_COUNT) break;

    idx_buf[0] = '1' + (char)idx;
    idx_buf[1] = '\0';

    if(is_focused)
    {
      sl_disp_fill_rect(0, y, SL_DISP_WIDTH, UI_CHAR_H, 0);
      sl_disp_draw_string(UI_ARROW_X, (uint16_t)y, ">", &sl_default_font, color);
    }
    else
    {
      sl_disp_draw_string(UI_ARROW_X, (uint16_t)y, " ", &sl_default_font, color);
    }

    sl_disp_draw_string(UI_INDEX_X, (uint16_t)y, idx_buf, &sl_default_font, color);
    sl_disp_draw_string(UI_LABEL_X, (uint16_t)y, SL_LANG(s_item_ids[idx]), &sl_font_chinese, color);
  }
}

static int uimm_proc(sl_Page *self, const sl_Event *evt)
{
  (void)self;

  switch(evt->type)
  {
    case SL_EVT_KEY_UP:
      if(s_cursor > 0)
      {
        s_cursor--;
      }
      else
      {
        s_cursor = UI_MENU_ITEM_COUNT - 1;
      }
      s_scroll_off = (s_cursor <= 1) ? 0 : (UI_MENU_ITEM_COUNT - 2);
      sl_page_request_redraw();
      break;

    case SL_EVT_KEY_DOWN:
      if(s_cursor < UI_MENU_ITEM_COUNT - 1)
      {
        s_cursor++;
      }
      else
      {
        s_cursor = 0;
      }
      s_scroll_off = (s_cursor <= 1) ? 0 : (UI_MENU_ITEM_COUNT - 2);
      sl_page_request_redraw();
      break;

    case SL_EVT_KEY_ENTER:
      s_selected = s_cursor;
      return 1;

    case SL_EVT_KEY_BACK:
      s_back_to_idle = true;
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
    s_menu_page.data = 0;
    s_menu_page.arg = 0;
  }
  return &s_menu_page;
}

int ui_main_menu_consume_selected(void)
{
  int v = s_selected;
  s_selected = UI_MENU_ITEM_NONE;
  return v;
}

bool ui_main_menu_consume_back_to_idle(void)
{
  bool v = s_back_to_idle;
  s_back_to_idle = false;
  return v;
}
