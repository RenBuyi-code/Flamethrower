#ifndef UI_SAFETY_PAGE_H
#define UI_SAFETY_PAGE_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include <stdbool.h>
#include <stdint.h>

sl_Page *ui_safety_page_get(void);
void ui_safety_page_set_tilt_ref(int16_t *tilt_enable);
int ui_safety_page_consume_tilt_changed(void);

#endif
