/**
 * @file    sl_event.c
 * @brief   SlateUI 事件系统实现 —— 环形队列与 UI 语义事件分发
 *
 * 本文件实现两层事件机制的核心逻辑：
 *   1) 原始输入事件环形队列：使用 volatile 头尾指针，保证 ISR 与主循环间安全共享。
 *      空/满判断：队列满条件为 (tail+1) mod SIZE == head，牺牲一个槽位区分空与满。
 *   2) UI 语义事件多订阅者分发：静态回调数组，同步调用所有已注册处理器。
 *
 * 线程安全说明：
 *   - sl_event_post() 可在 ISR 中调用（仅写 tail）；
 *   - sl_event_get() 仅在主循环中调用（仅写 head）；
 *   - 单生产者-单消费者模型，无需关中断。
 */

#include "../inc/sl_event.h"
#include <string.h>

/* 编译期检查：队列大小必须为 2 的幂且大于 1 */
#if (SL_EVENT_QUEUE_SIZE <= 1) || ((SL_EVENT_QUEUE_SIZE & (SL_EVENT_QUEUE_SIZE - 1)) != 0)
#error "SL_EVENT_QUEUE_SIZE must be a power of two and greater than 1."
#endif

/* ======================== 原始事件环形队列 ======================== */

/** @brief 事件环形缓冲区（静态分配） */
static sl_Event queue[SL_EVENT_QUEUE_SIZE];

/** @brief 出队指针（主循环使用），volatile 防止编译器优化 */
static volatile int head = 0;

/** @brief 入队指针（ISR 使用），volatile 防止编译器优化 */
static volatile int tail = 0;

/**
 * @brief  初始化原始事件队列
 * @note   将头尾指针归零，清空缓冲区；
 *         必须在 sl_page_manager_init 之前调用
 */
void sl_event_queue_init(void) {
    head = tail = 0;
    memset(queue, 0, sizeof(queue));
}

/**
 * @brief  向队列尾部投递一个原始事件
 * @param  event  指向待投递事件的指针
 * @retval true   投递成功
 * @retval false  队列已满，事件被丢弃
 * @note   通常在 ISR 或 port 层输入驱动中调用；
 *         使用位掩码取模（SIZE 为 2 的幂），避免除法运算
 */
bool sl_event_post(const sl_Event *event) {
    int next = (tail + 1) & (SL_EVENT_QUEUE_SIZE - 1);
    if (next == head) {
        return false;
    }
    queue[tail] = *event;
    tail = next;
    return true;
}

/**
 * @brief  从队列头部取出一个原始事件
 * @param  event  输出：存放取出的事件
 * @retval true   取出成功
 * @retval false  队列为空
 * @note   仅在主循环中调用
 */
bool sl_event_get(sl_Event *event) {
    if (head == tail) {
        return false;
    }
    *event = queue[head];
    head = (head + 1) & (SL_EVENT_QUEUE_SIZE - 1);
    return true;
}

/* ======================== UI 语义事件分发 ======================== */

/** @brief UI 语义事件处理器静态数组 */
static sl_UiEventHandler g_ui_handlers[SL_UI_EVENT_MAX_HANDLERS];

/** @brief 当前已注册的处理器数量 */
static int g_ui_handler_count = 0;

/**
 * @brief  设置唯一的 UI 语义事件处理器（兼容旧接口）
 * @param  handler  回调函数指针，NULL 则清除所有处理器
 * @note   调用此函数会清空所有已订阅的处理器，然后注册唯一一个；
 *         推荐使用 sl_ui_event_subscribe() 替代
 */
void sl_ui_event_set_handler(sl_UiEventHandler handler) {
    g_ui_handler_count = 0;
    memset(g_ui_handlers, 0, sizeof(g_ui_handlers));
    if (handler) {
        g_ui_handlers[0] = handler;
        g_ui_handler_count = 1;
    }
}

/**
 * @brief  订阅 UI 语义事件（多订阅者模式）
 * @param  handler  回调函数指针
 * @retval true   订阅成功（或已存在相同订阅，不会重复注册）
 * @retval false  订阅者数组已满，或 handler 为 NULL
 */
bool sl_ui_event_subscribe(sl_UiEventHandler handler) {
    if (!handler) return false;
    if (g_ui_handler_count >= SL_UI_EVENT_MAX_HANDLERS) return false;
    for (int i = 0; i < g_ui_handler_count; i++) {
        if (g_ui_handlers[i] == handler) return true;
    }
    g_ui_handlers[g_ui_handler_count++] = handler;
    return true;
}

/**
 * @brief  取消订阅 UI 语义事件
 * @param  handler  之前订阅的回调函数指针
 * @note   使用末尾元素覆盖被删除元素（O(1) 删除），
 *         不保证回调顺序，但 MCU 场景下顺序无关紧要
 */
void sl_ui_event_unsubscribe(sl_UiEventHandler handler) {
    if (!handler) return;
    for (int i = 0; i < g_ui_handler_count; i++) {
        if (g_ui_handlers[i] == handler) {
            g_ui_handlers[i] = g_ui_handlers[g_ui_handler_count - 1];
            g_ui_handlers[g_ui_handler_count - 1] = NULL;
            g_ui_handler_count--;
            return;
        }
    }
}

/**
 * @brief  投递一个 UI 语义事件给所有订阅者
 * @param  evt  指向语义事件的指针
 * @note   同步调用所有已注册回调，调用顺序与订阅顺序一致；
 *         若 evt 为 NULL 或 type 为 SL_UI_EVT_NONE，则忽略
 */
void sl_ui_event_post(const sl_UiEvent *evt) {
    if (!evt || evt->type == SL_UI_EVT_NONE) return;
    for (int i = 0; i < g_ui_handler_count; i++) {
        if (g_ui_handlers[i]) {
            g_ui_handlers[i](evt);
        }
    }
}
