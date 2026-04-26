# SlateUI

`SlateUI` is a lightweight MCU GUI framework in pure C (static memory + event-driven loop).

## Layered Directory Structure

```text
middleware/SlateUI/
|- AGENTS.md
|- README.md
|- README_zh-CN.md
|- core/
|  |- inc/
|  |  |- sl_display.h
|  |  |- sl_event.h
|  |  |- sl_key_repeat.h
|  |  |- sl_language.h
|  |  |- sl_page.h
|  |  |- sl_page_manager.h
|  |  `- sl_tween.h
|  `- src/
|     |- sl_display.c
|     |- sl_event.c
|     |- sl_key_repeat.c
|     |- sl_language.c
|     |- sl_page_manager.c
|     `- sl_tween.c
|- widgets/
|  |- inc/
|  |  |- sl_widget.h
|  |  |- sl_icon.h
|  |  |- sl_icon_item.h
|  |  |- sl_horizontal_menu.h
|  |  |- sl_label.h
|  |  |- sl_linear_layout.h
|  |  |- sl_list_view.h
|  |  `- sl_progress_bar.h
|  `- src/
|     |- sl_widget.c
|     |- sl_icon.c
|     |- sl_icon_item.c
|     |- sl_horizontal_menu.c
|     |- sl_label.c
|     |- sl_linear_layout.c
|     |- sl_list_view.c
|     `- sl_progress_bar.c
|- font/
|  |- sl_font.h
|  |- sl_font_ascii_16x16.h
|  |- sl_font_ascii_16x16.c
|  |- sl_font_chinese_16x16.h
|  `- sl_font_chinese_16x16.c
|- menu/
|  |- inc/
|  |  |- sl_menu_model.h
|  |  `- sl_menu_page.h
|  `- src/
|     |- sl_menu_model.c
|     `- sl_menu_page.c
`- port/
   |- sl_port.h
   `- sl_port.c
```

## Layer Responsibilities

- `core`: event queue, page stack, rendering buffer, language table, UI semantic events.
- `widgets`: reusable UI controls and widget tree dispatch, widget ID and find-by-id.
- `menu`: data-driven menu model and menu page lifecycle. MenuModel describes the menu tree; MenuPage renders it using ListView.
- `font`: font abstraction and built-in glyph data.
- `port`: platform hooks (display IO, input source, optional async TX).

## Widget ID

Every widget now carries a stable `id` field (`sl_widget_id_t`, `uint16_t`).  
`0` (`SL_WIDGET_ID_NONE`) means unnamed; valid IDs start at 1.

```c
void           sl_widget_set_id(sl_Widget *widget, sl_widget_id_t id);
sl_widget_id_t sl_widget_get_id(const sl_Widget *widget);
sl_Widget     *sl_widget_find_by_id(sl_Widget *root, sl_widget_id_t id);
const sl_Widget *sl_widget_find_by_id_const(const sl_Widget *root, sl_widget_id_t id);
```

IDs should be unique within the same page. `find_by_id` returns the first match (depth-first).

## UI Semantic Events

On top of raw input events (`sl_Event`), SlateUI now provides a higher-level
UI semantic event layer (`sl_UiEvent`) for front-end/back-end separation.

### Event Types

| Enum Value | Meaning |
|---|---|
| `SL_UI_EVT_FOCUS_CHANGED` | Focus moved to a different item |
| `SL_UI_EVT_ENTER_ITEM` | User confirmed/entered the current item |
| `SL_UI_EVT_BACK` | User requested to go back |
| `SL_UI_EVT_VALUE_CHANGED` | An editable value changed (not yet committed) |
| `SL_UI_EVT_VALUE_COMMIT` | An editable value was confirmed/committed |
| `SL_UI_EVT_ACTION_TRIGGERED` | A menu action was triggered |

### Event Payload

```c
typedef struct {
    sl_ui_event_type_t type;
    sl_widget_id_t     widget_id;
    sl_action_id_t     action_id;
    int32_t            value;
    int32_t            value_prev;
    void              *context;
} sl_UiEvent;
```

### Receiving UI Events

SlateUI supports multiple subscribers via a static handler array
(`SL_UI_EVENT_MAX_HANDLERS`, default 4, overridable at compile time).

```c
void presenter_handler(const sl_UiEvent *evt) { /* ... */ }
void logger_handler(const sl_UiEvent *evt)   { /* ... */ }

sl_ui_event_subscribe(presenter_handler);
sl_ui_event_subscribe(logger_handler);
```

Legacy single-handler API (clears all subscribers, then registers one):

```c
sl_ui_event_set_handler(my_handler);
```

To remove a subscriber:

```c
sl_ui_event_unsubscribe(logger_handler);
```

`sl_ui_event_subscribe()` returns `false` if the handler array is full.
Duplicate subscriptions are silently ignored (returns `true`).

### Widgets that Emit UI Events

- `sl_list_view`: emits `FOCUS_CHANGED` on cursor move, `ENTER_ITEM` on ENTER key.

## Action ID

`sl_action_id_t` (`uint16_t`) is a stable action identifier used in menu models.
`SL_ACTION_NONE` (`0`) means no action. Application code defines its own enum values starting from 1.

## MenuModel

MenuModel is a data-driven menu description layer. A single static data array
describes an entire settings page — no hand-written page logic needed.

### Menu Item Types

| Type | Display | Behavior |
|---|---|---|
| `SL_MENU_SUB_MENU` | "Title >" | Enters child menu page on ENTER |
| `SL_MENU_TOGGLE` | "Title ON/OFF" | Toggles boolean on ENTER |
| `SL_MENU_CHOICE` | "Title Option" | Cycles through options on ENTER |
| `SL_MENU_ACTION` | "Title" | Calls `on_action` callback on ENTER |
| `SL_MENU_VALUE` | "Title 123" | Enters inline edit mode (UP/DOWN to change) |

### Menu Item Structure

```c
typedef struct {
    const char       *text;          /* display text (externally owned) */
    sl_MenuItemType   type;          /* item type enum */
    int16_t           value;         /* current value (TOGGLE:0/1, CHOICE:index, VALUE:number) */
    int16_t           min;           /* minimum (VALUE type) */
    int16_t           max;           /* maximum (VALUE type) */
    const char      **choices;       /* option text array (CHOICE type, externally owned) */
    int16_t           choice_count;  /* option count (CHOICE type) */
    sl_MenuActionCb   on_action;     /* action callback (ACTION type) */
    sl_MenuPageModel *sub;           /* child menu model (SUB_MENU type) */
    sl_MenuValueGetter get_value;    /* optional: external value reader (NULL=read item->value) */
    sl_MenuValueSetter set_value;    /* optional: external value writer (NULL=write item->value) */
} sl_MenuItem;
```

### Value Getter/Setter Decoupling (Optional)

By default, `value` is stored directly in `sl_MenuItem`. For business-layer
ownership (e.g., reading from hardware registers or EEPROM), set `get_value`
and/or `set_value` callbacks:

```c
/* Example: temperature value backed by global state */
int16_t temp_getter(const sl_MenuItem *item) { (void)item; return g_target_temp; }
void temp_setter(sl_MenuItem *item, int16_t val) {
    (void)item;
    if (val >= 0 && val <= 500) g_target_temp = val;
}

sl_MenuItem temp_item = {
    .text = "Target Temp",
    .type = SL_MENU_VALUE,
    .min = 0, .max = 500,
    .get_value = temp_getter,
    .set_value = temp_setter,
};
```

When callbacks are `NULL` (default), behavior is unchanged — fully backward compatible.

### Menu Page Model Structure

```c
typedef struct {
    const char       *title;      /* page title text (externally owned) */
    const sl_MenuItem *items;     /* menu item array (externally owned) */
    int16_t           item_count; /* number of menu items */
} sl_MenuPageModel;
```

### Model Helper Functions

```c
void        sl_menu_item_toggle(sl_MenuItem *item);              /* toggle TOGGLE item */
void        sl_menu_item_next_choice(sl_MenuItem *item);         /* cycle CHOICE item */
const char* sl_menu_item_get_choice_text(const sl_MenuItem *item);/* get CHOICE display text */
const char* sl_menu_item_get_toggle_text(const sl_MenuItem *item);/* get TOGGLE display text */
int16_t     sl_menu_item_get_value(const sl_MenuItem *item);     /* read value (via getter or direct) */
void        sl_menu_item_set_value(sl_MenuItem *item, int16_t v);/* write value (via setter or direct) */
```

## MenuPage

MenuPage is a `sl_Page` implementation that renders a `sl_MenuPageModel` model
using a `sl_ListView` widget. It handles cursor navigation, inline value editing,
submenu entry, and processes interactions through the page's presenter callback.

### Usage

MenuPage uses object pool allocation (no manual init needed):

```c
/* 1. Define menu model (static data) */
static const sl_MenuItem settings_items[] = {
    { .text = "Brightness", .type = SL_MENU_TOGGLE, .value = 1 },
    { .text = "Mode",       .type = SL_MENU_CHOICE, .value = 0,
      .choices = (const char *[]){"Auto", "Manual"}, .choice_count = 2 },
    { .text = "Target Temp", .type = SL_MENU_VALUE, .value = 200, .min = 0, .max = 500 },
    { .text = "Save",       .type = SL_MENU_ACTION, .on_action = on_save },
};

static const sl_MenuPageModel settings_model = {
    .title = "Settings",
    .items = settings_items,
    .item_count = sizeof(settings_items) / sizeof(settings_items[0]),
};

/* 2. Allocate and enter */
sl_Page *menu_page = sl_menu_page_alloc(&settings_model);
if (menu_page) {
    sl_page_enter(menu_page);
}
```

Pool size is `SL_MENU_PAGE_POOL_SIZE` (default 4, overridable at compile time).
Slots are automatically released when the page exits.

### Internal Data Structure

```c
typedef struct {
    const sl_MenuPageModel *model;      /* current menu model (read-only) */
    sl_ListView             list_view;  /* list view widget instance */
    sl_key_repeat_t         key_repeat; /* key repeat generator instance */
    uint8_t                 editing;    /* value edit mode flag (1=editing, 0=browsing) */
} sl_MenuPageData;
```

### Interaction Flow

| Interaction | Behavior |
|---|---|
| UP/DOWN | Move cursor (via ListView, emits FOCUS_CHANGED) |
| ENTER on SUB_MENU | Allocate child page, push to stack |
| ENTER on TOGGLE | Toggle 0↔1, refresh display |
| ENTER on CHOICE | Cycle to next option, refresh display |
| ENTER on ACTION | Call `item->on_action(item)` |
| ENTER on VALUE | Enter edit mode (`editing=1`) |
| UP/DOWN in edit mode | Adjust value within `[min, max]`, refresh |
| ENTER/BACK in edit mode | Exit edit mode |
| BACK (browsing) | Return 1 → page manager pops this page |

## Presenter

Each `sl_Page` can optionally carry a `presenter` callback that receives UI
semantic events together with the page pointer, enabling the Presenter to
update UI state or call Service functions.

```c
typedef void (*sl_PagePresenter)(const sl_UiEvent *evt, sl_Page *page);
```

### Usage

```c
void settings_presenter(const sl_UiEvent *evt, sl_Page *page) {
    if (evt->type == SL_UI_EVT_ENTER_ITEM) {
        sl_MenuPageData *data = (sl_MenuPageData *)page->data;
        const sl_MenuItem *item = &data->model->items[evt->value];
        /* React to item selection: sync to service layer, etc. */
    }
}

page->presenter = settings_presenter;
```

MenuPage automatically calls `page->presenter` for each UI semantic event.
For custom pages, call `sl_ui_event_post()` and the presenter fires automatically,
or manually invoke `page->presenter` after posting.

### Dual-Channel Event Delivery

UI semantic events are delivered through two channels simultaneously:

1. **Global subscribers** (`sl_ui_event_subscribe`) — for logging, analytics, or
   cross-page coordination.
2. **Page Presenter** (`page->presenter`) — for page-specific business logic,
   receives the page pointer for direct state updates.

## Service Boundary

Service functions live in application code, not in SlateUI. They must follow
these rules:

- **Naming**: `<domain>_service_<verb>(<params>)` — e.g. `temp_service_set_target(int32_t)`
- **No UI types**: must not accept widget/page pointers or return UI state
- **No direct global access from UI**: UI accesses business data only through Service
- **Return style**: `0` = success, negative = error

SlateUI framework code never calls Service functions directly.

## Page Navigation with Arguments

Pages can receive arguments when entered, enabling data passing between pages
without global variables.

### `sl_page_enter_with`

```c
void sl_page_enter_with(sl_Page *new_page, void *arg);
```

Sets `new_page->arg = arg` before calling `init`, so the init callback can
read `self->arg` to configure the page. `arg` is a one-shot parameter —
the page typically copies what it needs into its own `data` during init.

The existing `sl_page_enter(page)` is equivalent to `sl_page_enter_with(page, NULL)`.

### Example

```c
typedef struct { int target_temp; int mode; } settings_params_t;

/* Caller */
settings_params_t params = { .target_temp = 200, .mode = 1 };
sl_page_enter_with(settings_page, &params);

/* Settings page init */
static void settings_init(sl_Page *self) {
    settings_params_t *p = (settings_params_t *)self->arg;
    /* copy into page-private state */
}

/* Return value: use UI semantic events */
sl_UiEvent evt = { .type = SL_UI_EVT_VALUE_COMMIT, .value = new_temp, ... };
sl_ui_event_post(&evt);
```

## New Widget Primitives

- `sl_icon`: draws a monochrome 1bpp bitmap inside widget bounds.
- `sl_icon_item`: renders icon + label with focus and selected states.
- `sl_horizontal_menu`: manages horizontally arranged icon items and selection.

## Label Scrolling

`sl_Label` supports automatic horizontal scrolling when text exceeds widget width.

```c
sl_Label title_label;
sl_label_init(&title_label, 0, 0, 128, 10, "Very Long Setting Name", font, 1, SL_LABEL_ALIGN_LEFT);
sl_label_set_scroll(&title_label, SL_LABEL_SCROLL_AUTO);
```

Call `sl_label_tick(&label)` from your main loop or timer callback to advance
the scroll position. Speed and pause duration are configurable at compile time
(`SL_LABEL_SCROLL_SPEED`, `SL_LABEL_SCROLL_PAUSE_FRAMES`).

## Key Repeat (Long-Press Acceleration)

`sl_key_repeat_t` generates repeated direction-key events when a key is held,
with an accelerating interval curve.

```c
sl_key_repeat_t key_rep;
sl_key_repeat_init(&key_rep);

/* In event handler: */
sl_key_repeat_on_event(&key_rep, &event);

/* In timer tick (e.g. every 10 ms): */
sl_key_repeat_tick(&key_rep, 10);
```

Timing is configurable: `SL_KEY_REPEAT_DELAY_MS` (initial delay),
`SL_KEY_REPEAT_INTERVAL_MS` (first repeat interval),
`SL_KEY_REPEAT_MIN_INTERVAL_MS` (fastest interval),
`SL_KEY_REPEAT_ACCEL_STEP_MS` (acceleration step).

## Tween (Lightweight Animation)

`sl_tween_t` provides pure-integer interpolation with common easing curves.
No floating point, no dynamic allocation.

```c
sl_tween_t tween;
sl_tween_start(&tween, 0, 100, 300, SL_TWEEN_EASE_OUT);

/* In timer tick: */
sl_tween_tick(&tween, delta_ms);
int32_t value = sl_tween_get_value(&tween);
```

Available curves: `SL_TWEEN_LINEAR`, `SL_TWEEN_EASE_IN`,
`SL_TWEEN_EASE_OUT`, `SL_TWEEN_EASE_IN_OUT`.

## Drawing Helpers

```c
void sl_disp_draw_hline(int x, int y, int w, int color);
void sl_disp_draw_vline(int x, int y, int h, int color);
void sl_disp_draw_title_bar(const char *title, const void *font);
void sl_disp_draw_status_bar(int y, const char *text, const void *font);
```

`draw_title_bar` fills a 10px-high bar at y=0 with inverted text.
`draw_status_bar` does the same at a given y position.

## Cursor Animation

`sl_ListView` integrates `sl_tween_t` for smooth cursor sliding. When the
cursor moves, the highlight bar animates from the old position to the new
position using an ease-out curve.

Animation duration is configurable: `SL_LIST_CURSOR_ANIM_MS` (default 120ms).

Call `sl_list_view_tick(lv, delta_ms)` from your main loop or timer callback
to advance the animation.

## Page Transitions

Page enter/go-back now features a horizontal slide transition. The old page
slides out while the new page slides in, using an ease-out curve.

Transition duration is configurable: `SL_PAGE_TRANSITION_MS` (default 150ms).

Call `sl_page_manager_tick(delta_ms)` from your timer callback to drive the
transition. During a transition, event processing is locked to prevent
accidental input.

## Scroll Indicator

`sl_ListView` can display a 2px-wide scroll indicator on the right side,
showing the current viewport position within the full list.

```c
sl_list_view_set_scrollbar(&list_view, 1);
```

The thumb height and position are calculated proportionally from the visible
count and scroll offset.

## Input Notes

- `sl_horizontal_menu` handles `SL_EVT_KEY_LEFT` and `SL_EVT_KEY_RIGHT`.
- `SL_EVT_KEY_UP` and `SL_EVT_KEY_DOWN` are also accepted as fallback navigation.

## Build Integration Notes

After this refactor, ensure your include paths cover:

- `middleware/SlateUI/core/inc`
- `middleware/SlateUI/widgets/inc`
- `middleware/SlateUI/menu/inc`
- `middleware/SlateUI/font`
- `middleware/SlateUI/port`

And compile sources from:

- `middleware/SlateUI/core/src`
- `middleware/SlateUI/widgets/src`
- `middleware/SlateUI/menu/src`
- `middleware/SlateUI/font`
- `middleware/SlateUI/port`

## Port Hooks Summary

From `port/sl_port.h`:

```c
void sl_port_init(void);
void sl_port_input_init(void);
void sl_port_poll_input(void);
void sl_hw_set_window(int x, int y, int w, int h);
void sl_hw_send_pixels(const uint8_t *data, int len);
```

Optional switches:

```c
#define SL_USE_PINGPONG_BUF 0
#define SL_PORT_USE_ASYNC_TX 0
```

Recommended for DMA/asynchronous display TX:

- `SL_USE_PINGPONG_BUF=1`
- `SL_PORT_USE_ASYNC_TX=1`
