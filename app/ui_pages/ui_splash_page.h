#ifndef UI_SPLASH_PAGE_H
#define UI_SPLASH_PAGE_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include <stdbool.h>

sl_Page *ui_splash_page_get(void);
void ui_splash_page_tick(void);
bool ui_splash_page_is_done(void);

#endif