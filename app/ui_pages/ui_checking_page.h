#ifndef UI_CHECKING_PAGE_H
#define UI_CHECKING_PAGE_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include "../../domain/machine_state.h"
#include <stdbool.h>
#include <stdint.h>

sl_Page *ui_checking_page_get(void);
void ui_checking_page_update(machine_state_t state, uint8_t pressure_pct, uint32_t fault_mask);
bool ui_checking_page_is_done(void);

#endif
