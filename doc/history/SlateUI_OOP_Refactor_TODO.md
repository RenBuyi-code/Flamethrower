# SlateUI OOP Refactor TODO

## Goal

Keep SlateUI aligned with five rules:

- object-oriented, but not over-engineered
- simple to use from application code
- easy to extend with new pages
- easy to port to bare metal or RTOS projects
- easy to read and review

The practical success condition is simple:
creating a new page should usually take 3-4 steps, and should not require
editing a big pile of UI glue in `freertos_app.c`.

## What Was Missing

Before this round, the main gaps were:

- app-facing API was still too close to `sl_page_manager_*`
- page state style was inconsistent
- UI pages still depended on project-local `extern` save functions
- text layout helpers were too weak, so pages still hand-laid strings
- several notes and comments had encoding damage, which hurt reviewability

## Current Status

### Done

- [x] `T0-1` Facade API
  - `sl_ui_init()`
  - `sl_ui_run_once()`
  - `sl_ui_post_key()`
  - `sl_ui_navigate()`
  - `sl_ui_go_back()`
  - `sl_ui_current_page()`

- [x] `T0-2` Page registry
  - static page registration table
  - page lookup by name

- [x] `T0-3` Page private state convention
  - common pages now use `self->data`
  - helper macro `SL_PAGE_DATA_AS(type, self)`

- [x] `T0-4` Snapshot / service layer
  - `ui_machine_snapshot_t`
  - `ui_service_set_machine_snapshot()`
  - `ui_service_get_machine_snapshot()`

- [x] `T0-5` Self-navigation
  - pages navigate and go back by themselves
  - main loop no longer owns page-specific routing logic

- [x] `T0-7` Text layout helpers, phase 1
  - `sl_text_measure_width()`
  - `sl_text_draw_center()`
  - `sl_text_draw_segments()`

- [x] `T0-9` UI independent time base, project side
  - `sl_ui_tick_up()` is now the active UI time source in the app
  - the old external page timing glue was removed from `freertos_app.c`

- [x] Page-to-app save decoupling, phase 1
  - pages now save through `ui_services`
  - they no longer depend directly on `freertos_app.c` extern callbacks

- [x] Encoding cleanup, phase 1
  - core refactor notes rewritten cleanly
  - `app_core.*` and `ui_services.*` comments rewritten for review
  - `ui_idle_page.c` rewritten cleanly

### Still Worth Doing

- [ ] `T0-6` Lifecycle refinement
  - only if a real page needs `resume/pause/result`
  - do not add lifecycle hooks just for symmetry

- [ ] `T0-7` Text layout helpers, phase 2
  - optional column helper
  - optional reusable menu-row helper

- [ ] `T0-8` Declarative menu / form layer
  - target the common settings page shape
  - keep it table-driven and static-memory only

- [ ] `T0-10` Migration demo
  - one tiny demo that uses only new API
  - good as onboarding material and regression proof

- [ ] Documentation sweep
  - finish replacing old `sl_page_manager_tick(delta_ms)` guidance in docs
  - add one short "bare metal integration" example

## Design Rules We Should Keep

### 1. Keep the facade small

Application code should mainly touch:

```c
sl_ui_init(...)
sl_ui_run_once()
sl_ui_post_key(...)
sl_ui_navigate(...)
sl_ui_go_back()
sl_ui_current_page()
```

### 2. Keep pages self-contained

A page should mostly answer:

- what to draw
- what to do on keys
- where to navigate
- which service snapshot to read

### 3. Keep project-specific layers outside SlateUI

`app_core` and `ui_services` are useful in this project, but they are not
SlateUI requirements. A bare-metal adopter should still be able to use
SlateUI directly.

### 4. Prefer clearer code over clever abstractions

If a helper does not reduce repeated page code in a visible way, do not add it.

## Acceptance Check

The refactor is on track if:

- `freertos_app.c` keeps getting thinner
- a new page no longer needs special-case glue in the main loop
- settings pages are easier to build than before
- page code reads locally instead of forcing cross-file archaeology
- another engineer can understand the layering quickly during review
