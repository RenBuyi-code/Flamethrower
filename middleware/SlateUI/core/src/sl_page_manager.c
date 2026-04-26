/**
 * @file    sl_page_manager.c
 * @brief   SlateUI 页面栈管理器实现
 *
 * 本文件实现基于栈的页面导航与滑动过渡动画：
 *   · 页面栈采用静态指针数组，最大深度 SL_MAX_PAGE_DEPTH。
 *   · 页面进入时播放从右滑入动画，返回时播放从左滑入动画。
 *   · 过渡动画期间冻结事件分发，防止动画期间误操作。
 *   · 过渡动画使用 sl_tween_t 驱动，由 sl_page_manager_tick() 推进。
 */

#include "../inc/sl_page_manager.h"
#include "../inc/sl_display.h"
#include "../inc/sl_event.h"
#include <string.h>

/* ======================== 页面栈状态 ======================== */

/** @brief 页面栈（静态指针数组，LIFO） */
static sl_Page *page_stack[SL_MAX_PAGE_DEPTH];

/** @brief 栈顶索引（-1 表示空栈） */
static int stack_top = -1;

/** @brief 重绘请求标志（1=需要重绘，0=无需重绘） */
static uint8_t redraw_pending = 0;

/* ======================== 过渡动画状态 ======================== */

/** @brief 过渡动画插值器 */
static sl_tween_t trans_tween;

/** @brief 当前过渡方向（NONE 表示无过渡） */
static sl_transition_dir_t trans_dir = SL_TRANS_NONE;

/** @brief 过渡动画中被替换的旧页面指针（用于双页同屏绘制） */
static sl_Page *trans_old_page = NULL;

static sl_Page* current_page(void);

/**
 * @brief  UI 语义事件到当前页 Presenter 的桥接分发
 * @param  evt  UI 语义事件
 */
static void page_manager_presenter_dispatch(const sl_UiEvent *evt) {
    sl_Page *cur = current_page();
    if ((cur != NULL) && (cur->presenter != NULL)) {
        cur->presenter(evt, cur);
    }
}

/* ======================== 内部辅助函数 ======================== */

/**
 * @brief  获取当前栈顶页面
 * @retval 指向栈顶页面的指针，栈空时返回 NULL
 */
static sl_Page* current_page(void) {
    if (stack_top >= 0 && stack_top < SL_MAX_PAGE_DEPTH) {
        return page_stack[stack_top];
    }
    return NULL;
}

/* ======================== 页面管理器接口实现 ======================== */

/**
 * @brief  初始化页面管理器并推入根页面
 * @param  root_page  指向根页面实例
 * @note   依次初始化显示模块、事件队列、页面栈状态，
 *         然后推入根页面（触发 init + 过渡动画）
 */
void sl_page_manager_init(sl_Page *root_page) {
    sl_disp_init();
    sl_event_queue_init();
    sl_ui_event_subscribe(page_manager_presenter_dispatch);
    stack_top = -1;
    redraw_pending = 0;
    trans_dir = SL_TRANS_NONE;
    trans_old_page = NULL;
    memset(&trans_tween, 0, sizeof(trans_tween));
    if (root_page) {
        sl_page_enter(root_page);
    }
}

/**
 * @brief  页面管理器主循环处理
 *
 * 执行流程：
 *   1. 若过渡动画进行中，跳过事件分发（冻结输入）；
 *   2. 从事件队列取出所有事件，依次分发给栈顶页面的 proc()；
 *   3. 若 proc() 返回 1，自动执行 go_back 并返回；
 *   4. 若有事件或脏标记，调用栈顶页面的 draw()；
 *   5. 调用 sl_disp_flush() 刷新物理屏幕。
 */
void sl_page_manager_process(void) {
    sl_Event evt;
    sl_Page *cur = current_page();
    uint8_t has_event = 0;
    if (!cur) return;

    if (trans_dir != SL_TRANS_NONE) return;

    while (sl_event_get(&evt)) {
        has_event = 1;
        if (cur->proc && cur->proc(cur, &evt)) {
            sl_page_go_back();
            return;
        }
    }

    if (!has_event && !redraw_pending) {
        return;
    }

    cur = current_page();
    if (cur && cur->draw) {
        cur->draw(cur);
    }

    sl_disp_flush();
    redraw_pending = 0;
}

/**
 * @brief  绘制一帧过渡动画
 *
 * 根据过渡方向和当前插值偏移量，将旧页面和新页面
 * 以水平滑动方式同时绘制到显存中：
 *   - ENTER: 旧页面向左滑出，新页面从右滑入
 *   - GO_BACK: 当前页面向右滑出，旧页面从左滑入
 *
 * @note   每帧先清空显存，再分别设置偏移绘制两个页面，
 *         最后 flush 到物理屏幕
 */
static void draw_transition_frame(void) {
    int offset = (int)sl_tween_get_value(&trans_tween);

    sl_disp_init();

    if (trans_dir == SL_TRANS_ENTER) {
        if (trans_old_page && trans_old_page->draw) {
            sl_disp_set_offset(-SL_DISP_WIDTH + offset, 0);
            trans_old_page->draw(trans_old_page);
            sl_disp_set_offset(0, 0);
        }
        sl_Page *new_page = current_page();
        if (new_page && new_page->draw) {
            sl_disp_set_offset(offset, 0);
            new_page->draw(new_page);
            sl_disp_set_offset(0, 0);
        }
    } else if (trans_dir == SL_TRANS_GO_BACK) {
        sl_Page *back_page = current_page();
        if (back_page && back_page->draw) {
            sl_disp_set_offset(-SL_DISP_WIDTH + offset, 0);
            back_page->draw(back_page);
            sl_disp_set_offset(0, 0);
        }
        if (trans_old_page && trans_old_page->draw) {
            sl_disp_set_offset(offset, 0);
            trans_old_page->draw(trans_old_page);
            sl_disp_set_offset(0, 0);
        }
    }

    sl_disp_flush();
}

/**
 * @brief  推入新页面并传递入口参数
 * @param  new_page  指向新页面实例
 * @param  arg       传递给新页面的参数指针，存入 new_page->arg
 * @note   执行顺序：
 *         1. 设置 arg 字段
 *         2. 调用新页面的 init() 回调
 *         3. 记录旧页面，将新页面压入栈顶
 *         4. 启动进入过渡动画（从右滑入）
 */
void sl_page_enter_with(sl_Page *new_page, void *arg) {
    if (!new_page || stack_top >= SL_MAX_PAGE_DEPTH - 1) return;

    new_page->arg = arg;

    if (new_page->init) {
        new_page->init(new_page);
    }

    trans_old_page = current_page();
    page_stack[++stack_top] = new_page;

    if (trans_old_page == NULL) {
        /* First page on boot: draw directly, avoid transition artifacts. */
        trans_dir = SL_TRANS_NONE;
        redraw_pending = 1;
    } else {
        trans_dir = SL_TRANS_ENTER;
        /* Enter animation: new page slides from right (x=+W) to center (x=0). */
        sl_tween_start(&trans_tween, SL_DISP_WIDTH, 0,
                       SL_PAGE_TRANSITION_MS, SL_TWEEN_EASE_OUT);
    }
}

/**
 * @brief  推入新页面（无参数）
 * @param  new_page  指向新页面实例
 * @note   等价于 sl_page_enter_with(new_page, NULL)
 */
void sl_page_enter(sl_Page *new_page) {
    sl_page_enter_with(new_page, NULL);
}

/**
 * @brief  弹出栈顶页面，返回上一页面
 * @note   执行顺序：
 *         1. 调用当前页面的 exit() 回调
 *         2. 记录旧页面，栈顶指针减一
 *         3. 启动返回过渡动画（从左滑入）
 *         若栈中只剩根页面（stack_top <= 0），则忽略此调用
 */
void sl_page_go_back(void) {
    if (stack_top <= 0) return;

    sl_Page *cur = current_page();
    if (cur && cur->exit) {
        cur->exit(cur);
    }

    trans_old_page = cur;
    stack_top--;

    trans_dir = SL_TRANS_GO_BACK;
    sl_tween_start(&trans_tween, 0, SL_DISP_WIDTH,
                   SL_PAGE_TRANSITION_MS, SL_TWEEN_EASE_OUT);
}

/**
 * @brief  请求重绘当前页面
 * @note   设置脏标记，下次 process() 时将调用 draw()
 */
void sl_page_request_redraw(void) {
    redraw_pending = 1;
}

/**
 * @brief  页面管理器时钟节拍
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   当过渡动画进行中时：
 *         1. 推进插值动画时间
 *         2. 绘制一帧过渡画面
 *         3. 动画完成后清除过渡状态，恢复事件分发
 */
void sl_page_manager_tick(uint16_t delta_ms) {
    if (trans_dir == SL_TRANS_NONE) return;

    sl_tween_tick(&trans_tween, delta_ms);
    draw_transition_frame();

    if (sl_tween_is_finished(&trans_tween)) {
        trans_dir = SL_TRANS_NONE;
        trans_old_page = NULL;
        redraw_pending = 0;
    }
}
