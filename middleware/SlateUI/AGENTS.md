# SlateUI Local Protocol (Higher Priority in This Directory)

This file applies to `middleware/SlateUI/**` and overrides broader repo guidance where needed.

## 1) Mandatory Pre-Edit Steps

1. Read the matching header before changing any `core/src/*.c`, `widgets/src/*.c`, or `menu/src/*.c` file.
2. Check both README files for API/docs impact:
   - `README.md`
   - `README_zh-CN.md`
3. Keep changes minimal and scoped to the requested issue.

## 2) File Ownership and Boundaries

- `port/sl_port.c/.h`:
  - Hardware abstraction only.
  - No widget/page business logic.
- `core/inc/*.h`, `widgets/inc/*.h`, `menu/inc/*.h`, `font/*.h`:
  - Public API contracts.
  - Must stay consistent with implementation.
- `core/src/*.c`:
  - Core framework implementation.
  - Must not call MCU HAL directly (except through port layer).
- `widgets/src/*.c`:
  - Widget implementations only.
  - Must not call MCU HAL directly (except through display/port abstractions).
- `menu/src/*.c`:
  - MenuModel data description and MenuPage lifecycle.
  - May use core APIs (event, display, page_manager) and widget APIs (list_view).
  - Must not call MCU HAL directly.
  - Must not contain application-specific business logic.
- `font/*`:
  - Font abstraction and glyph data only.

## 3) Architectural Invariants

- Keep event-driven flow:
  - producer: `sl_event_post()`
  - consumer: `sl_event_get()` in page-manager loop
- Keep UI semantic event flow:
  - widgets/pages produce `sl_UiEvent` via `sl_ui_event_post()` / `post_ui_event()`
  - Presenter callbacks consume `sl_UiEvent` (via `sl_ui_event_subscribe` or `page->presenter`)
- Keep render flow:
  - draw into framebuffer via `sl_disp_*`
  - flush via `sl_disp_flush()`
  - hardware send only through `sl_hw_*`
- Do not add dynamic allocation in core paths (`event`, `page_manager`, `display`, `widget`).

## 4) Service Boundary Rules

Application-side Service functions must follow these rules:

- **Naming**: `<domain>_service_<verb>(<params>)` — e.g. `temp_service_set_target(int32_t value)`
- **No UI types**: Service functions must not accept widget/page pointers or return UI state.
- **No global variable access from UI**: UI code must access business data only through Service functions.
- **Return style**: `0` = success, negative = error.
- **Location**: Service implementations belong in application code, not in SlateUI.

SlateUI framework code must never call Service functions directly.

## 5) API and Pairing Rules

- Any exported function in `core/src` or `widgets/src` must have a declaration in matching header.
- Any header API removal must be reflected in source implementation.
- Do not leave `.h`/`.c` half-implemented pairs.

## 6) Current Project-Specific Constraints

- Default font is **8x16 ASCII** (sl_font_ascii_16x16.c) with full 95-character coverage.
- Chinese font **16x16 GB2312** (sl_font_chinese_16x16.c) provides 64 common characters for LCD12232.
- For 122×32 screens, 8x16 ASCII shows ~2 rows; 16x16 Chinese shows ~1 row. Plan layout carefully.

## 7) Style and Safety

- Use `sl_` prefix consistently.
- Preserve existing naming and style in surrounding files.
- Avoid unrelated refactors.
- Keep MCU-friendly performance and memory behavior.

## 8) Done Criteria for SlateUI Changes

- Header/source consistency checked.
- No layer violation introduced.
- README files updated when public behavior changes.
- Residual limitations explicitly documented in final handoff.
