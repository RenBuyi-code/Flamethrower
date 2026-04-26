#include "../inc/sl_page_registry.h"
#include <string.h>

static sl_PageEntry s_entries[SL_UI_MAX_REGISTERED_PAGES];
static uint8_t s_entry_count;

void sl_ui_registry_reset(void)
{
    uint8_t i;

    for (i = 0; i < SL_UI_MAX_REGISTERED_PAGES; i++) {
        s_entries[i].name = NULL;
        s_entries[i].factory = NULL;
    }
    s_entry_count = 0U;
}

bool sl_ui_register(const char *name, sl_PageFactory factory)
{
    uint8_t i;

    if ((name == NULL) || (factory == NULL)) {
        return false;
    }

    for (i = 0; i < s_entry_count; i++) {
        if ((s_entries[i].name != NULL) && (strcmp(s_entries[i].name, name) == 0)) {
            s_entries[i].factory = factory;
            return true;
        }
    }

    if (s_entry_count >= SL_UI_MAX_REGISTERED_PAGES) {
        return false;
    }

    s_entries[s_entry_count].name = name;
    s_entries[s_entry_count].factory = factory;
    s_entry_count++;
    return true;
}

bool sl_ui_register_pages(const sl_PageEntry *entries, uint8_t count)
{
    uint8_t i;

    if ((entries == NULL) || (count == 0U)) {
        return false;
    }

    for (i = 0U; i < count; i++) {
        if (sl_ui_register(entries[i].name, entries[i].factory) == false) {
            return false;
        }
    }

    return true;
}

sl_Page *sl_ui_resolve(const char *name)
{
    uint8_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < s_entry_count; i++) {
        if ((s_entries[i].name != NULL) && (strcmp(s_entries[i].name, name) == 0)) {
            return (s_entries[i].factory != NULL) ? s_entries[i].factory() : NULL;
        }
    }

    return NULL;
}
