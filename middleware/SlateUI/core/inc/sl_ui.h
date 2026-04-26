#ifndef SL_UI_H
#define SL_UI_H

#include "sl_page_registry.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SL_UI_KEY_NONE = 0,
    SL_UI_KEY_UP,
    SL_UI_KEY_DOWN,
    SL_UI_KEY_LEFT,
    SL_UI_KEY_RIGHT,
    SL_UI_KEY_ENTER,
    SL_UI_KEY_BACK
} sl_ui_key_t;

/*
 * SlateUI facade API
 *
 * Purpose:
 * - keep application-facing usage small and stable
 * - hide page manager / event queue details from app code
 * - provide one clear entry for tick, input and navigation
 */

void sl_ui_init(const char *root_page);
void sl_ui_run_once(void);

/* UI clock */
void sl_ui_tick_up(void);
uint32_t sl_ui_get_tick(void);

/* Input */
void sl_ui_post_key(sl_ui_key_t key, bool pressed);

/* Navigation */
bool sl_ui_navigate(const char *target);
bool sl_ui_navigate_with(const char *target, void *arg);
void sl_ui_go_back(void);

/* Rendering helpers */
void sl_ui_request_redraw(void);
const char *sl_ui_current_page(void);

#endif
