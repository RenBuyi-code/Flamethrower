/**
 * @file    sl_widget.h
 * @brief   SlateUI 控件基类 —— 控件树与事件分发
 *
 * 本模块定义 SlateUI 所有控件的公共基类 sl_Widget，
 * 提供控件树管理（父子/兄弟关系）、绘制分发和事件路由：
 *   · 控件树采用 first-child / next-sibling 结构，
 *     类似 Android View 树，支持任意深度的嵌套布局。
 *   · 绘制时通过 sl_widget_draw_tree() 递归遍历整棵树，
 *     每个控件的 draw() 接收父级累积偏移量。
 *   · 事件分发通过 sl_widget_dispatch_event() 深度优先遍历，
 *     第一个返回 true 的控件消费该事件。
 *   · 支持控件 ID 查找（sl_widget_find_by_id），
 *     用于跨控件通信和测试定位。
 *
 * 控件标志位：
 *   - VISIBLE:   控件可见，参与绘制
 *   - FOCUSABLE: 控件可接收焦点
 *   - FOCUSED:   控件当前持有焦点
 */

#ifndef SL_WIDGET_H
#define SL_WIDGET_H

#include <stdint.h>
#include <stdbool.h>
#include "../../core/inc/sl_event.h"

/* ======================== 前向声明与回调类型 ======================== */

/** @brief 控件结构体前向声明 */
struct sl_Widget;

/**
 * @brief  控件绘制回调函数类型
 * @param  self      指向当前控件实例
 * @param  offset_x  父级累积 X 偏移（像素）
 * @param  offset_y  父级累积 Y 偏移（像素）
 * @note   绘制时应使用 (self->x + offset_x, self->y + offset_y) 作为屏幕坐标
 */
typedef void (*sl_WidgetDraw)(struct sl_Widget *self, int offset_x, int offset_y);

/**
 * @brief  控件事件处理回调函数类型
 * @param  self   指向当前控件实例
 * @param  event  指向待处理的原始输入事件
 * @retval true   事件已消费，不再向下传递
 * @retval false  事件未消费，继续传递给兄弟/父控件
 */
typedef bool (*sl_WidgetProc)(struct sl_Widget *self, const sl_Event *event);

/* ======================== 控件基类结构体 ======================== */

/**
 * @brief  控件基类结构体
 *
 * 所有 SlateUI 控件（Label、ListView、Icon 等）均以此为第一个成员，
 * 实现类似 C++ 继承的多态效果。控件通过 first-child / next-sibling
 * 指针构成树形结构。
 */
typedef struct sl_Widget {
    sl_widget_id_t   id;            /**< 控件唯一标识符（0=未命名） */
    int16_t          x;             /**< 控件左上角 X 坐标（相对父级） */
    int16_t          y;             /**< 控件左上角 Y 坐标（相对父级） */
    int16_t          w;             /**< 控件宽度（像素） */
    int16_t          h;             /**< 控件高度（像素） */
    struct sl_Widget *parent;       /**< 父控件指针（根控件为 NULL） */
    struct sl_Widget *first_child;  /**< 第一个子控件指针（无子控件为 NULL） */
    struct sl_Widget *next_sibling; /**< 下一个兄弟控件指针（末尾为 NULL） */
    sl_WidgetDraw    draw;          /**< 绘制回调函数 */
    sl_WidgetProc    proc;          /**< 事件处理回调函数 */
    uint8_t          flags;         /**< 控件标志位（SL_WIDGET_FLAG_xxx） */
    void            *user_data;     /**< 用户自定义数据指针 */
} sl_Widget;

/* ======================== 控件标志位定义 ======================== */

/** @brief 控件可见标志（参与绘制） */
#define SL_WIDGET_FLAG_VISIBLE   0x01

/** @brief 控件可聚焦标志（可接收焦点） */
#define SL_WIDGET_FLAG_FOCUSABLE 0x02

/** @brief 控件已聚焦标志（当前持有焦点） */
#define SL_WIDGET_FLAG_FOCUSED   0x04

/* ======================== 控件树管理接口 ======================== */

/**
 * @brief  初始化控件基类
 * @param  w       指向控件实例
 * @param  x       控件左上角 X 坐标
 * @param  y       控件左上角 Y 坐标
 * @param  w_px    控件宽度（像素）
 * @param  h_px    控件高度（像素）
 * @param  draw    绘制回调函数（可为 NULL）
 * @param  proc    事件处理回调函数（可为 NULL）
 * @note   初始化后 flags=SL_WIDGET_FLAG_VISIBLE，无父/子/兄弟
 */
void  sl_widget_init(sl_Widget *w, int x, int y, int w_px, int h_px,
                     sl_WidgetDraw draw, sl_WidgetProc proc);

/**
 * @brief  将子控件添加到父控件的子控件链表末尾
 * @param  parent  指向父控件
 * @param  child   指向待添加的子控件
 * @note   子控件的 parent 指针自动设置为 parent
 */
void  sl_widget_add_child(sl_Widget *parent, sl_Widget *child);

/**
 * @brief  从父控件的子控件链表中移除指定子控件
 * @param  parent  指向父控件
 * @param  child   指向待移除的子控件
 * @note   同时将 child->parent 置为 NULL
 */
void  sl_widget_remove_child(sl_Widget *parent, sl_Widget *child);

/**
 * @brief  递归绘制整棵控件树
 * @param  root   指向根控件
 * @param  offx   根控件 X 偏移（通常为 0）
 * @param  offy   根控件 Y 偏移（通常为 0）
 * @note   仅绘制 flags 包含 VISIBLE 的控件；
 *         递归遍历子控件时累加偏移量
 */
void  sl_widget_draw_tree(sl_Widget *root, int offx, int offy);

/**
 * @brief  向控件树分发原始输入事件
 * @param  root   指向根控件
 * @param  event  指向待分发的事件
 * @retval true   事件已被某个控件消费
 * @retval false  事件未被任何控件消费
 * @note   深度优先遍历，子控件优先；第一个返回 true 的控件消费事件
 */
bool  sl_widget_dispatch_event(sl_Widget *root, const sl_Event *event);

/* ======================== 控件 ID 查找接口 ======================== */

/**
 * @brief  设置控件 ID
 * @param  widget  指向控件实例
 * @param  id      控件标识符（同一页面内应唯一）
 */
void              sl_widget_set_id(sl_Widget *widget, sl_widget_id_t id);

/**
 * @brief  获取控件 ID
 * @param  widget  指向控件实例（只读）
 * @retval 控件标识符
 */
sl_widget_id_t    sl_widget_get_id(const sl_Widget *widget);

/**
 * @brief  根据控件 ID 在控件树中查找控件（可修改）
 * @param  root  指向根控件
 * @param  id    目标控件 ID
 * @retval 指向找到的控件指针，未找到返回 NULL
 */
sl_Widget        *sl_widget_find_by_id(sl_Widget *root, sl_widget_id_t id);

/**
 * @brief  根据控件 ID 在控件树中查找控件（只读）
 * @param  root  指向根控件（只读）
 * @param  id    目标控件 ID
 * @retval 指向找到的控件只读指针，未找到返回 NULL
 */
const sl_Widget  *sl_widget_find_by_id_const(const sl_Widget *root, sl_widget_id_t id);

#endif
