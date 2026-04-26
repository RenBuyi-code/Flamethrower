/**
 * @file    sl_page_manager.h
 * @brief   SlateUI 页面栈管理器
 *
 * 本模块实现基于栈的页面导航与过渡动画，类似 Android Activity 栈：
 *   · 维护一个后进先出 (LIFO) 的页面栈，最大深度 SL_MAX_PAGE_DEPTH。
 *   · 支持页面进入 (enter) 与返回 (go_back) 两种导航操作。
 *   · 页面切换时可播放滑动过渡动画，动画期间冻结事件分发。
 *   · 提供 tick 驱动接口，由应用主循环周期性调用以推进动画。
 *
 * 典型用法：
 *   sl_page_manager_init(&main_page);   // 初始化并推入根页面
 *   while (1) {
 *       sl_page_manager_process();       // 事件分发 + 绘制
 *       sl_page_manager_tick(delta_ms);  // 推进过渡动画
 *   }
 */

#ifndef SL_PAGE_MANAGER_H
#define SL_PAGE_MANAGER_H

#include "sl_page.h"
#include "sl_tween.h"

/* ======================== 页面栈配置 ======================== */

/** @brief 页面栈最大深度（超出后 sl_page_enter 将忽略新页面） */
#define SL_MAX_PAGE_DEPTH  8

/**
 * @brief  页面过渡动画默认时长（毫秒）
 * @note   可在编译选项中覆盖，设为 0 则禁用过渡动画
 */
#ifndef SL_PAGE_TRANSITION_MS
#define SL_PAGE_TRANSITION_MS 150
#endif

/* ======================== 过渡动画类型定义 ======================== */

/**
 * @brief  页面过渡方向枚举
 */
typedef enum {
    SL_TRANS_NONE    = 0,  /**< 无过渡，直接切换 */
    SL_TRANS_ENTER,        /**< 进入新页面（从右滑入） */
    SL_TRANS_GO_BACK       /**< 返回上一页面（从左滑入） */
} sl_transition_dir_t;

/* ======================== 页面管理器接口 ======================== */

/**
 * @brief  初始化页面管理器并推入根页面
 * @param  root_page  指向根页面实例（首页），将立即调用其 init + draw
 * @note   必须在 sl_event_queue_init 之后调用；
 *         根页面不会被 sl_page_go_back 弹出（栈底保护）
 */
void sl_page_manager_init(sl_Page *root_page);

/**
 * @brief  页面管理器主循环处理
 * @note   每帧调用一次，执行以下步骤：
 *         1. 若过渡动画进行中，跳过事件分发
 *         2. 从事件队列取出事件，分发给栈顶页面的 proc()
 *         3. 若 proc() 返回 1，自动执行 go_back
 *         4. 调用栈顶页面的 draw()（如有脏标记）
 *         5. 调用 sl_disp_flush() 刷新物理屏幕
 */
void sl_page_manager_process(void);

/**
 * @brief  推入新页面（无参数）
 * @param  new_page  指向新页面实例
 * @note   新页面的 arg 字段为 NULL；
 *         若需传参请使用 sl_page_enter_with()
 */
void sl_page_enter(sl_Page *new_page);

/**
 * @brief  推入新页面并传递入口参数
 * @param  new_page  指向新页面实例
 * @param  arg       传递给新页面的参数指针，存入 new_page->arg
 * @note   页面的 init() 回调可通过 self->arg 获取此参数；
 *         典型场景：菜单页面传递选中的菜单模型指针
 */
void sl_page_enter_with(sl_Page *new_page, void *arg);

/**
 * @brief  弹出栈顶页面，返回上一页面
 * @note   若栈中只剩根页面，则忽略此调用（栈底保护）；
 *         弹出前会调用当前页面的 exit() 回调
 */
void sl_page_go_back(void);

/**
 * @brief  请求重绘当前页面
 * @note   设置脏标记，下次 process() 时将调用 draw()
 */
void sl_page_request_redraw(void);

/**
 * @brief  页面管理器时钟节拍
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   由应用主循环周期性调用，用于推进过渡动画；
 *         动画完成后自动清除过渡状态并恢复事件分发
 */
void sl_page_manager_tick(uint16_t delta_ms);

#endif
