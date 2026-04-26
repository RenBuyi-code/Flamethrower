/**
 * @file    sl_linear_layout.h
 * @brief   SlateUI 线性布局控件
 *
 * 本模块实现水平/垂直线性布局，自动排列子控件：
 *   · 支持水平 (HORIZONTAL) 和垂直 (VERTICAL) 两种方向。
 *   · 子控件按添加顺序依次排列，间距由 spacing 控制。
 *   · 调用 sl_linear_layout_apply() 后，自动计算并设置
 *     每个子控件的 x/y 坐标。
 *   · 布局从控件的 (x, y) 位置开始，依次偏移子控件的宽度/高度 + spacing。
 */

#ifndef SL_LINEAR_LAYOUT_H
#define SL_LINEAR_LAYOUT_H

#include "sl_widget.h"

/* ======================== 布局方向枚举 ======================== */

/**
 * @brief  线性布局方向枚举
 */
typedef enum {
    SL_LAYOUT_HORIZONTAL = 0,  /**< 水平布局（子控件从左到右排列） */
    SL_LAYOUT_VERTICAL         /**< 垂直布局（子控件从上到下排列） */
} sl_LayoutOrientation;

/* ======================== 线性布局控件结构体 ======================== */

/**
 * @brief  线性布局控件结构体
 *
 * 继承 sl_Widget 基类，增加布局方向和间距属性。
 * 子控件通过 sl_widget_add_child() 添加。
 */
typedef struct {
    sl_Widget            base;     /**< 控件基类（必须为第一个成员） */
    sl_LayoutOrientation orient;   /**< 布局方向 */
    uint8_t              spacing;  /**< 子控件间距（像素） */
} sl_LinearLayout;

/* ======================== 线性布局接口 ======================== */

/**
 * @brief  初始化线性布局控件
 * @param  layout   指向布局实例
 * @param  x        控件左上角 X 坐标
 * @param  y        控件左上角 Y 坐标
 * @param  w        控件宽度（像素）
 * @param  h        控件高度（像素）
 * @param  orient   布局方向 (SL_LAYOUT_HORIZONTAL / SL_LAYOUT_VERTICAL)
 * @param  spacing  子控件间距（像素）
 */
void sl_linear_layout_init(sl_LinearLayout *layout, int x, int y, int w, int h,
                           sl_LayoutOrientation orient, uint8_t spacing);

/**
 * @brief  应用线性布局，自动排列所有子控件
 * @param  layout  指向布局实例
 * @note   遍历所有子控件，根据布局方向和间距计算并设置
 *         每个子控件的 x/y 坐标；
 *         应在添加/移除子控件后调用
 */
void sl_linear_layout_apply(sl_LinearLayout *layout);

#endif
