#include "ui_setting_page.h"
#include "../ui_services.h"
#include "../../middleware/SlateUI/core/inc/sl_display.h"
#include "../../middleware/SlateUI/core/inc/sl_ui.h"
#include "../../middleware/SlateUI/core/inc/sl_language.h"
#include "../../middleware/SlateUI/font/sl_font_chinese_16x16.h"
#include <string.h>
#include <stdio.h>

#define UI_CHAR_H          16

static const char *s_dmx_mode_choices[] = { "2CH", "6CH" };

static ui_setting_cfg_t s_dmx_cfg;
static ui_setting_cfg_t s_pressure_cfg;
static sl_Page s_dmx_page;
static sl_Page s_pressure_page;

typedef struct
{
  const ui_setting_cfg_t *cfg;
  int focus;
  int editing;
  int16_t edit_value;
} ui_setting_priv_t;

static ui_setting_priv_t s_dmx_priv;
static ui_setting_priv_t s_pressure_priv;

static ui_setting_priv_t *uis_state(sl_Page *self)
{
  return SL_PAGE_DATA_AS(ui_setting_priv_t, self);
}

static void uis_format_value(char *buf, int buf_size, const ui_setting_field_t *f, int16_t val)
{
  if(f->value == 0)
  {
    strncpy(buf, "--", (size_t)(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return;
  }
  if(f->choices != 0 && f->choice_cnt > 0)
  {
    int idx = (int)val;
    if(idx < 0) idx = 0;
    if(idx >= f->choice_cnt) idx = f->choice_cnt - 1;
    strncpy(buf, f->choices[idx], (size_t)(buf_size - 1));
    buf[buf_size - 1] = '\0';
  }
  else
  {
    snprintf(buf, (size_t)buf_size, "%d", (int)val);
  }
}

static void setting_draw(sl_Page *self)
{
  ui_setting_priv_t *priv = uis_state(self);
  const ui_setting_cfg_t *cfg = priv->cfg;
  char val_buf[16];
  char line_buf[20];
  int i;

  sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 32, 1);

  for(i = 0; i < 2; i++)
  {
    const ui_setting_field_t *f = &cfg->fields[i];
    int y = i * UI_CHAR_H;
    int is_focused = (priv->focus == i);
    int is_editing = (priv->editing != 0) && is_focused;
    int16_t display_val;

    if(is_editing)
    {
      display_val = priv->edit_value;
    }
    else
    {
      display_val = *(f->value);
    }

    uis_format_value(val_buf, sizeof(val_buf), f, display_val);

    if(is_editing)
    {
      snprintf(line_buf, sizeof(line_buf), "%s: %s", SL_LANG(f->label_id), val_buf);

      sl_disp_fill_rect(0, y, SL_DISP_WIDTH, UI_CHAR_H, 0);
      sl_disp_draw_string(0, (uint16_t)y, line_buf, &sl_font_chinese, 1);
    }
    else if(is_focused)
    {
      snprintf(line_buf, sizeof(line_buf), "%s: [%s]", SL_LANG(f->label_id), val_buf);

      sl_disp_fill_rect(0, y, SL_DISP_WIDTH, UI_CHAR_H, 0);
      sl_disp_draw_string(0, (uint16_t)y, line_buf, &sl_font_chinese, 1);
    }
    else
    {
      snprintf(line_buf, sizeof(line_buf), "%s: %s", SL_LANG(f->label_id), val_buf);

      sl_disp_draw_string(0, (uint16_t)y, line_buf, &sl_font_chinese, 0);
    }
  }
}

static int uis_clamp_value(const ui_setting_field_t *f, int16_t val)
{
  if(val < f->min_val) val = f->min_val;
  if(val > f->max_val) val = f->max_val;
  return (int)val;
}

static int setting_proc(sl_Page *self, const sl_Event *evt)
{
  ui_setting_priv_t *priv = uis_state(self);
  const ui_setting_cfg_t *cfg = priv->cfg;

  if(priv->editing != 0)
  {
    switch(evt->type)
    {
      case SL_EVT_KEY_UP:
      {
        const ui_setting_field_t *f = &cfg->fields[priv->focus];
        if(f->choices != 0 && f->choice_cnt > 0)
        {
          priv->edit_value = (int16_t)(((int)priv->edit_value + 1) % (int)f->choice_cnt);
        }
        else
        {
          priv->edit_value = (int16_t)uis_clamp_value(f, (int16_t)((int)priv->edit_value + (int)f->step));
        }
        sl_ui_request_redraw();
        break;
      }

      case SL_EVT_KEY_DOWN:
      {
        const ui_setting_field_t *f = &cfg->fields[priv->focus];
        if(f->choices != 0 && f->choice_cnt > 0)
        {
          priv->edit_value = (int16_t)(((int)priv->edit_value - 1 + (int)f->choice_cnt) % (int)f->choice_cnt);
        }
        else
        {
          priv->edit_value = (int16_t)uis_clamp_value(f, (int16_t)((int)priv->edit_value - (int)f->step));
        }
        sl_ui_request_redraw();
        break;
      }

      case SL_EVT_KEY_ENTER:
      {
        const ui_setting_field_t *f = &cfg->fields[priv->focus];
        if(f->value != 0)
        {
          *(f->value) = priv->edit_value;
          if(f->on_save != 0)
          {
            f->on_save(priv->edit_value);
          }
        }
        priv->editing = 0;
        sl_ui_request_redraw();
        break;
      }

      case SL_EVT_KEY_BACK:
        priv->editing = 0;
        sl_ui_request_redraw();
        break;

      default:
        break;
    }
  }
  else
  {
    switch(evt->type)
    {
      case SL_EVT_KEY_UP:
        priv->focus = (priv->focus == 0) ? 1 : 0;
        sl_ui_request_redraw();
        break;

      case SL_EVT_KEY_DOWN:
        priv->focus = (priv->focus == 1) ? 0 : 1;
        sl_ui_request_redraw();
        break;

      case SL_EVT_KEY_ENTER:
      {
        const ui_setting_field_t *f = &cfg->fields[priv->focus];
        priv->editing = 1;
        priv->edit_value = (f->value != 0) ? *(f->value) : 0;
        sl_ui_request_redraw();
        break;
      }

      case SL_EVT_KEY_BACK:
        return 1;

      default:
        break;
    }
  }

  return 0;
}

static void setting_init(sl_Page *self)
{
  ui_setting_priv_t *priv = uis_state(self);

  priv->focus = 0;
  priv->editing = 0;
  priv->edit_value = 0;
}

static void dmx_cfg_init(void)
{
  s_dmx_cfg.title = "DMX Set";

  s_dmx_cfg.fields[0].label = 0;
  s_dmx_cfg.fields[0].label_id = SL_STR_ADDR;
  s_dmx_cfg.fields[0].min_val = 1;
  s_dmx_cfg.fields[0].max_val = 512;
  s_dmx_cfg.fields[0].step = 1;
  s_dmx_cfg.fields[0].choices = 0;
  s_dmx_cfg.fields[0].choice_cnt = 0;
  s_dmx_cfg.fields[0].on_save = ui_service_save_dmx_addr;

  s_dmx_cfg.fields[1].label = 0;
  s_dmx_cfg.fields[1].label_id = SL_STR_MODE;
  s_dmx_cfg.fields[1].min_val = 0;
  s_dmx_cfg.fields[1].max_val = 1;
  s_dmx_cfg.fields[1].step = 1;
  s_dmx_cfg.fields[1].choices = s_dmx_mode_choices;
  s_dmx_cfg.fields[1].choice_cnt = 2;
  s_dmx_cfg.fields[1].on_save = ui_service_save_dmx_mode;
}

static void delay_cfg_init(void)
{
  s_pressure_cfg.title = "Delay Set";

  s_pressure_cfg.fields[0].label = 0;
  s_pressure_cfg.fields[0].label_id = SL_STR_IGN;
  s_pressure_cfg.fields[0].min_val = 0;
  s_pressure_cfg.fields[0].max_val = 120;
  s_pressure_cfg.fields[0].step = 1;
  s_pressure_cfg.fields[0].choices = 0;
  s_pressure_cfg.fields[0].choice_cnt = 0;
  s_pressure_cfg.fields[0].on_save = ui_service_save_ign_delay;

  s_pressure_cfg.fields[1].label = 0;
  s_pressure_cfg.fields[1].label_id = SL_STR_LOCK;
  s_pressure_cfg.fields[1].min_val = 0;
  s_pressure_cfg.fields[1].max_val = 120;
  s_pressure_cfg.fields[1].step = 1;
  s_pressure_cfg.fields[1].choices = 0;
  s_pressure_cfg.fields[1].choice_cnt = 0;
  s_pressure_cfg.fields[1].on_save = ui_service_save_lock_delay;
}

static sl_Page *uis_page_setup(sl_Page *page, ui_setting_priv_t *priv, ui_setting_cfg_t *cfg)
{
  if(page->init == 0)
  {
    page->name = cfg->title;
    page->init = setting_init;
    page->draw = setting_draw;
    page->proc = setting_proc;
    page->exit = 0;
    page->presenter = 0;
    page->tick = 0;
    page->data = priv;
    page->arg = 0;

    priv->cfg = cfg;
    priv->focus = 0;
    priv->editing = 0;
    priv->edit_value = 0;
  }
  return page;
}

void ui_setting_page_set_dmx_refs(int16_t *addr, int16_t *mode)
{
  dmx_cfg_init();
  s_dmx_cfg.fields[0].value = addr;
  s_dmx_cfg.fields[1].value = mode;
}

void ui_setting_page_set_delay_refs(int16_t *ign, int16_t *lock)
{
  delay_cfg_init();
  s_pressure_cfg.fields[0].value = ign;
  s_pressure_cfg.fields[1].value = lock;
}

sl_Page *ui_setting_page_get_dmx(void)
{
  dmx_cfg_init();
  return uis_page_setup(&s_dmx_page, &s_dmx_priv, &s_dmx_cfg);
}

sl_Page *ui_setting_page_get_delay(void)
{
  delay_cfg_init();
  return uis_page_setup(&s_pressure_page, &s_pressure_priv, &s_pressure_cfg);
}
