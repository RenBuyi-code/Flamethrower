#ifndef UI_SETTING_PAGE_H
#define UI_SETTING_PAGE_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include <stdint.h>
#include <stdbool.h>

typedef void (*ui_setting_save_cb)(int16_t value);

typedef struct
{
  const char       *label;
  uint8_t           label_id;
  int16_t          *value;
  int16_t           min_val;
  int16_t           max_val;
  int16_t           step;
  const char * const *choices;
  int16_t           choice_cnt;
  ui_setting_save_cb on_save;
} ui_setting_field_t;

typedef struct
{
  const char          *title;
  ui_setting_field_t   fields[2];
} ui_setting_cfg_t;

sl_Page *ui_setting_page_get_dmx(void);
sl_Page *ui_setting_page_get_pressure(void);
void ui_setting_page_set_dmx_refs(int16_t *addr, int16_t *mode);
void ui_setting_page_set_pressure_refs(int16_t *ign, int16_t *lock);

#endif
