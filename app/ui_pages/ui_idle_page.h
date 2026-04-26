#ifndef UI_IDLE_PAGE_H
#define UI_IDLE_PAGE_H

#include "../../middleware/SlateUI/core/inc/sl_page.h"
#include "../../domain/machine_state.h"
#include <stdint.h>
#include <stdbool.h>

sl_Page *ui_idle_page_get(void);
void ui_idle_page_update(machine_state_t state, bool dmx_online, bool pumping, uint8_t pressure_pct, uint32_t fault_mask, uint16_t dmx_address);
bool ui_idle_page_consume_enter_menu(void);

#endif
