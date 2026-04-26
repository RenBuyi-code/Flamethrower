#include "../inc/sl_ui.h"
#include "../inc/sl_event.h"
#include "../inc/sl_page_manager.h"

static volatile uint32_t s_ui_tick_ms;
static uint32_t s_ui_last_run_tick;
static bool s_ui_initialized;

static sl_EventType sl_ui_key_to_event(sl_ui_key_t key)
{
    switch (key) {
    case SL_UI_KEY_UP:
        return SL_EVT_KEY_UP;
    case SL_UI_KEY_DOWN:
        return SL_EVT_KEY_DOWN;
    case SL_UI_KEY_LEFT:
        return SL_EVT_KEY_LEFT;
    case SL_UI_KEY_RIGHT:
        return SL_EVT_KEY_RIGHT;
    case SL_UI_KEY_ENTER:
        return SL_EVT_KEY_ENTER;
    case SL_UI_KEY_BACK:
        return SL_EVT_KEY_BACK;
    case SL_UI_KEY_NONE:
    default:
        return SL_EVT_NONE;
    }
}

void sl_ui_init(const char *root_page)
{
    sl_Page *page;

    page = sl_ui_resolve(root_page);
    s_ui_tick_ms = 0U;
    s_ui_last_run_tick = 0U;
    s_ui_initialized = false;

    if (page == NULL) {
        return;
    }

    sl_page_manager_init(page);
    s_ui_initialized = true;
}

void sl_ui_run_once(void)
{
    uint32_t now_tick;
    uint32_t delta_tick;
    uint16_t delta_ms;
    sl_Page *page;

    if (s_ui_initialized == false) {
        return;
    }

    now_tick = s_ui_tick_ms;
    delta_tick = now_tick - s_ui_last_run_tick;
    s_ui_last_run_tick = now_tick;

    if (delta_tick > 0U) {
        delta_ms = (delta_tick > 0xFFFFU) ? 0xFFFFU : (uint16_t)delta_tick;
        page = sl_page_manager_current();
        if ((page != NULL) && (page->tick != NULL)) {
            page->tick(page, delta_ms);
        }
        sl_page_manager_tick(delta_ms);
    }

    sl_page_manager_process();
}

void sl_ui_tick_up(void)
{
    s_ui_tick_ms++;
}

uint32_t sl_ui_get_tick(void)
{
    return s_ui_tick_ms;
}

void sl_ui_post_key(sl_ui_key_t key, bool pressed)
{
    sl_Event evt;

    if (pressed == false) {
        return;
    }

    evt.type = sl_ui_key_to_event(key);
    evt.param = 0;
    evt.source = SL_EVT_SOURCE_RAW;

    if (evt.type != SL_EVT_NONE) {
        (void)sl_event_post(&evt);
    }
}

bool sl_ui_navigate(const char *target)
{
    return sl_ui_navigate_with(target, NULL);
}

bool sl_ui_navigate_with(const char *target, void *arg)
{
    sl_Page *page;

    if (s_ui_initialized == false) {
        return false;
    }

    page = sl_ui_resolve(target);
    if (page == NULL) {
        return false;
    }

    sl_page_enter_with(page, arg);
    return true;
}

void sl_ui_go_back(void)
{
    if (s_ui_initialized == false) {
        return;
    }
    sl_page_go_back();
}

void sl_ui_request_redraw(void)
{
    if (s_ui_initialized == false) {
        return;
    }
    sl_page_request_redraw();
}

const char *sl_ui_current_page(void)
{
    sl_Page *page;

    if (s_ui_initialized == false) {
        return NULL;
    }

    page = sl_page_manager_current();
    return (page != NULL) ? page->name : NULL;
}
