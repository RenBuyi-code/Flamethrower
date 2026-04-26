#ifndef SL_PAGE_REGISTRY_H
#define SL_PAGE_REGISTRY_H

#include "sl_page.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef SL_UI_MAX_REGISTERED_PAGES
#define SL_UI_MAX_REGISTERED_PAGES 16
#endif

typedef sl_Page *(*sl_PageFactory)(void);

typedef struct
{
    const char *name;
    sl_PageFactory factory;
} sl_PageEntry;

/*
 * Static page registry for SlateUI
 *
 * Purpose:
 * - resolve pages by name
 * - let app code register pages once, then navigate by string id
 * - avoid dynamic allocation
 *
 * Notes:
 * - this is framework infrastructure, not project-specific UI glue
 * - page factories are used so pages can keep private state internally
 */

void sl_ui_registry_reset(void);
bool sl_ui_register(const char *name, sl_PageFactory factory);
bool sl_ui_register_pages(const sl_PageEntry *entries, uint8_t count);
sl_Page *sl_ui_resolve(const char *name);

#endif
