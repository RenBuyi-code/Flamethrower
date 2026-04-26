/**
 * @file    sl_progress_bar.h
 * @brief   SlateUI 水平进度条控件
 *
 * 本模块实现水平进度条控件，用于显示数值进度：
 *   · 支持 min/max 范围设置，当前值自动钳位。
 *   · 前景色填充已进度部分，背景色填充未进度部分。
 *   · 适用于电池电量、音量、加载进度等场景。
 */

#ifndef SL_PROGRESS_BAR_H
#define SL_PROGRESS_BAR_H

#include "sl_widget.h"

/* ======================== 进度条控件结构体 ======================== */

/**
 * @brief  水平进度条控件结构体
 *
 * 继承 sl_Widget 基类，增加数值范围和颜色属性。
 * 绘制时根据 (value - min) / (max - min) 计算填充比例。
 */
typedef struct {
    sl_Widget base;       /**< 控件基类（必须为第一个成员） */
    int       min;        /**< 最小值 */
    int       max;        /**< 最大值 */
    int       value;      /**< 当前值（自动钳位到 min..max） */
    uint8_t   fg_color;   /**< 前景色（已进度部分，0=灭, 非0=亮） */
    uint8_t   bg_color;   /**< 背景色（未进度部分，0=灭, 非0=亮） */
} sl_ProgressBar;

/* ======================== 进度条控件接口 ======================== */

/**
 * @brief  初始化进度条控件
 * @param  bar       指向进度条实例
 * @param  x         控件左上角 X 坐标
 * @param  y         控件左上角 Y 坐标
 * @param  w         控件宽度（像素）
 * @param  h         控件高度（像素）
 * @param  min       最小值
 * @param  max       最大值
 * @param  fg_color  前景颜色（已进度部分）
 * @param  bg_color  背景颜色（未进度部分）
 */
void sl_progress_bar_init(sl_ProgressBar *bar, int x, int y, int w, int h,
                          int min, int max, uint8_t fg_color, uint8_t bg_color);

/**
 * @brief  设置当前值
 * @param  bar    指向进度条实例
 * @param  value  新的当前值（自动钳位到 min..max）
 */
void sl_progress_bar_set_value(sl_ProgressBar *bar, int value);

#endif
