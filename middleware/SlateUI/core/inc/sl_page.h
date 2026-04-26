#ifndef SL_PAGE_H
#define SL_PAGE_H

#include "sl_event.h"
#include <stdint.h>

typedef struct sl_Page sl_Page;

typedef int  (*sl_PageProc)(sl_Page *self, const sl_Event *event);
typedef void (*sl_PageInit)(sl_Page *self);
typedef void (*sl_PageDraw)(sl_Page *self);
typedef void (*sl_PageExit)(sl_Page *self);
typedef void (*sl_PagePresenter)(const sl_UiEvent *evt, sl_Page *page);
typedef void (*sl_PageTick)(sl_Page *self, uint16_t elapsed_ms);

#define SL_PAGE_DATA_AS(type, self) ((type *)((self)->data))

/*
 * SlateUI page object
 *
 * A page is the basic navigation unit in SlateUI.
 * It owns its callbacks, private state pointer and optional input argument.
 *
 * Lifecycle in current design:
 *   enter -> init -> [tick/proc/draw] -> exit
 *
 * Notes:
 * - `data` is page-private state owned by the page implementation.
 * - `arg` is an optional navigation argument passed by `sl_page_enter_with()`.
 * - `tick` is driven by the UI clock from task context, not ISR context.
 */
struct sl_Page {
    const char       *name;
    sl_PageInit       init;
    sl_PageDraw       draw;
    sl_PageProc       proc;
    sl_PageExit       exit;
    sl_PagePresenter  presenter;
    sl_PageTick       tick;
    void             *data;
    void             *arg;
};

#endif
