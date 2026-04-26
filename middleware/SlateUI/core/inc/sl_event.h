/**
 * @file    sl_event.h
 * @brief   SlateUI 事件系统 —— 原始输入事件与 UI 语义事件
 *
 * 本模块提供两层事件机制：
 *   1) 原始输入事件 (sl_Event)：由 port 层投递，主循环消费，
 *      用于按键、定时器等底层输入。
 *   2) UI 语义事件 (sl_UiEvent)：由控件/页面产生，Presenter 消费，
 *      用于前后台分离的业务逻辑传递。
 *
 * 事件队列采用静态环形缓冲区，无动态分配；
 * UI 语义事件采用静态回调数组，支持多订阅者。
 */

#ifndef SL_EVENT_H
#define SL_EVENT_H

#include <stdbool.h>
#include <stdint.h>

/* ======================== 基础类型定义 ======================== */

/**
 * @brief  控件标识符类型
 * @note   同一页面内应唯一，0 表示未命名
 */
typedef uint16_t sl_widget_id_t;

/** @brief 控件 ID 无效值（未命名） */
#define SL_WIDGET_ID_NONE ((sl_widget_id_t)0u)

/**
 * @brief  动作标识符类型
 * @note   用于菜单模型中表达"做什么"而非"怎么做"，
 *         应用层从 1 开始定义自己的枚举值
 */
typedef uint16_t sl_action_id_t;

/** @brief 动作 ID 无效值（无动作） */
#define SL_ACTION_NONE ((sl_action_id_t)0u)

/* ======================== 原始输入事件 ======================== */

/**
 * @brief  原始输入事件类型枚举
 */
typedef enum {
    SL_EVT_NONE = 0,        /**< 无事件 */
    SL_EVT_KEY_UP,          /**< 上键按下 */
    SL_EVT_KEY_DOWN,        /**< 下键按下 */
    SL_EVT_KEY_LEFT,        /**< 左键按下 */
    SL_EVT_KEY_RIGHT,       /**< 右键按下 */
    SL_EVT_KEY_ENTER,       /**< 确认键按下 */
    SL_EVT_KEY_BACK,        /**< 返回键按下 */
    SL_EVT_TIMER,           /**< 定时器事件 */
    SL_EVT_CUSTOM = 100     /**< 用户自定义事件起始值 */
} sl_EventType;

/** @brief 事件来源：原始硬件输入 */
#define SL_EVT_SOURCE_RAW     0

/** @brief 事件来源：按键重复模块生成的重复事件 */
#define SL_EVT_SOURCE_REPEAT  1

/**
 * @brief  原始输入事件结构体
 */
typedef struct {
    sl_EventType type;      /**< 事件类型 */
    int32_t      param;     /**< 事件参数（按键索引 / 定时器 ID 等） */
    uint8_t      source;    /**< 事件来源 (SL_EVT_SOURCE_RAW / REPEAT) */
} sl_Event;

/* ======================== UI 语义事件 ======================== */

/**
 * @brief  UI 语义事件类型枚举
 */
typedef enum {
    SL_UI_EVT_NONE = 0,            /**< 无语义事件 */
    SL_UI_EVT_FOCUS_CHANGED,       /**< 焦点移动到不同项 */
    SL_UI_EVT_ENTER_ITEM,          /**< 用户确认/进入当前项 */
    SL_UI_EVT_BACK,                /**< 用户请求返回 */
    SL_UI_EVT_VALUE_CHANGED,       /**< 可编辑值发生变化（尚未提交） */
    SL_UI_EVT_VALUE_COMMIT,        /**< 可编辑值已确认提交 */
    SL_UI_EVT_ACTION_TRIGGERED     /**< 菜单动作被触发 */
} sl_ui_event_type_t;

/**
 * @brief  UI 语义事件载荷结构体
 */
typedef struct {
    sl_ui_event_type_t type;       /**< 语义事件类型 */
    sl_widget_id_t     widget_id;  /**< 产生事件的控件 ID */
    sl_action_id_t     action_id;  /**< 关联的动作 ID */
    int32_t            value;      /**< 当前值 */
    int32_t            value_prev; /**< 变更前的值 */
    void              *context;    /**< 用户自定义上下文指针 */
} sl_UiEvent;

/* ======================== 队列与回调配置 ======================== */

/** @brief 原始事件环形队列容量（必须为 2 的幂） */
#define SL_EVENT_QUEUE_SIZE 16

/**
 * @brief  UI 语义事件最大订阅者数量
 * @note   可在编译选项中覆盖，默认 4
 */
#ifndef SL_UI_EVENT_MAX_HANDLERS
#define SL_UI_EVENT_MAX_HANDLERS 4
#endif

/**
 * @brief  UI 语义事件回调函数类型
 * @param  evt  指向语义事件的只读指针
 */
typedef void (*sl_UiEventHandler)(const sl_UiEvent *evt);

/* ======================== 原始事件队列接口 ======================== */

/**
 * @brief  初始化原始事件队列
 * @note   必须在 sl_page_manager_init 之前调用
 */
void sl_event_queue_init(void);

/**
 * @brief  向事件队列投递一个原始事件（ISR / 主循环安全）
 * @param  event  指向待投递事件的指针
 * @retval true   投递成功
 * @retval false  队列已满，事件丢失
 */
bool sl_event_post(const sl_Event *event);

/**
 * @brief  从事件队列取出一个原始事件
 * @param  event  输出：存放取出的事件
 * @retval true   取出成功
 * @retval false  队列为空
 */
bool sl_event_get(sl_Event *event);

/* ======================== UI 语义事件接口 ======================== */

/**
 * @brief  设置唯一的 UI 语义事件处理器（兼容旧接口）
 * @param  handler  回调函数指针，NULL 则清除
 * @note   调用此函数会清空所有已订阅的处理器，然后注册唯一一个
 */
void sl_ui_event_set_handler(sl_UiEventHandler handler);

/**
 * @brief  订阅 UI 语义事件（多订阅者模式）
 * @param  handler  回调函数指针
 * @retval true   订阅成功（或已存在相同订阅）
 * @retval false  订阅者数组已满
 */
bool sl_ui_event_subscribe(sl_UiEventHandler handler);

/**
 * @brief  取消订阅 UI 语义事件
 * @param  handler  之前订阅的回调函数指针
 */
void sl_ui_event_unsubscribe(sl_UiEventHandler handler);

/**
 * @brief  投递一个 UI 语义事件给所有订阅者
 * @param  evt  指向语义事件的指针
 * @note   同步调用所有已注册回调，调用顺序与订阅顺序一致
 */
void sl_ui_event_post(const sl_UiEvent *evt);

#endif
