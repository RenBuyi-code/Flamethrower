#ifndef UI_MAIN_MENU_H
#define UI_MAIN_MENU_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include <stdint.h>
#include <stdbool.h>

#define UI_MENU_ITEM_NONE       (-1)
#define UI_MENU_ITEM_DMX        0
#define UI_MENU_ITEM_PRESSURE   1
#define UI_MENU_ITEM_TILT       2
#define UI_MENU_ITEM_LANGUAGE   3
#define UI_MENU_ITEM_COUNT      4

sl_Page *ui_main_menu_get(void);

#endif
