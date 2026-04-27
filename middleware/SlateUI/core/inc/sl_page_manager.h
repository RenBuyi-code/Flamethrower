/**
 * @file    sl_page_manager.h
 * @brief   SlateUI page stack manager
 *
 * This module implements a small LIFO page stack with optional slide
 * transitions.
 *
 * Recommended usage for application code after the facade refactor:
 *   - call `sl_ui_tick_up()` from a fixed UI tick source
 *   - call `sl_ui_run_once()` from the main loop or UI task
 *
 * Direct `sl_page_manager_*` calls are still available for internal use and
 * compatibility, but new application code should prefer `sl_ui.h`.
 */

#ifndef SL_PAGE_MANAGER_H
#define SL_PAGE_MANAGER_H

#include "sl_page.h"
#include "sl_tween.h"

#define SL_MAX_PAGE_DEPTH  8

#ifndef SL_PAGE_TRANSITION_MS
#define SL_PAGE_TRANSITION_MS 0
#endif

typedef enum {
    SL_TRANS_NONE    = 0,
    SL_TRANS_ENTER,
    SL_TRANS_GO_BACK
} sl_transition_dir_t;

void sl_page_manager_init(sl_Page *root_page);
void sl_page_manager_process(void);
void sl_page_enter(sl_Page *new_page);
void sl_page_enter_with(sl_Page *new_page, void *arg);
void sl_page_go_back(void);
void sl_page_request_redraw(void);
void sl_page_manager_tick(uint16_t delta_ms);
sl_Page *sl_page_manager_current(void);

#endif
