/**
 * @file    sl_progress_bar.c
 * @brief   SlateUI 水平进度条控件实现
 *
 * 本文件实现水平进度条的绘制和值设置：
 *   · 绘制时先填充背景色，再根据 (value-min)/(max-min) 计算
 *     前景填充宽度并绘制前景色矩形。
 *   · 进度条为纯显示控件，不处理任何事件。
 *   · 值自动钳位到 [min, max] 范围，避免越界。
 *   · 当 min == max 时仅显示背景，不绘制前景。
 */

#include <stddef.h>
#include "../inc/sl_progress_bar.h"
#include "../../core/inc/sl_display.h"

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  进度条绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 *
 * 绘制流程：
 *   1. 填充整个控件区域为背景色；
 *   2. 若 max > min，计算前景填充宽度：
 *      fill_w = (value - min) * width / (max - min)；
 *   3. 绘制前景色矩形。
 */
static void bar_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_ProgressBar *bar = (sl_ProgressBar *)self;

    sl_disp_fill_rect(offset_x, offset_y, self->w, self->h, bar->bg_color);

    if (bar->max > bar->min) {
        int range = bar->max - bar->min;
        int val = bar->value;
        if (val < bar->min) val = bar->min;
        if (val > bar->max) val = bar->max;

        int fill_w = (val - bar->min) * self->w / range;
        if (fill_w > self->w) fill_w = self->w;
        if (fill_w > 0) {
            sl_disp_fill_rect(offset_x, offset_y, fill_w, self->h, bar->fg_color);
        }
    }
}

/* ======================== 进度条接口实现 ======================== */

/**
 * @brief  初始化进度条控件
 * @param  bar       进度条对象指针（用户静态分配）
 * @param  x         控件左上角 X 坐标
 * @param  y         控件左上角 Y 坐标
 * @param  w         控件宽度（像素）
 * @param  h         控件高度（像素）
 * @param  min       最小值（对应 0% 填充）
 * @param  max       最大值（对应 100% 填充）
 * @param  fg_color  前景颜色（已进度部分，0=灭, 非0=亮）
 * @param  bg_color  背景颜色（未进度部分，0=灭, 非0=亮）
 * @note   进度条不处理事件（proc=NULL），初始值为 min
 */
void sl_progress_bar_init(sl_ProgressBar *bar, int x, int y, int w, int h,
                          int min, int max,
                          uint8_t fg_color, uint8_t bg_color) {
    sl_widget_init(&bar->base, x, y, w, h, bar_draw, NULL);

    bar->min = min;
    bar->max = max;
    bar->value = min;
    bar->fg_color = fg_color;
    bar->bg_color = bg_color;
}

/**
 * @brief  设置进度条当前值
 * @param  bar    进度条对象指针
 * @param  value  当前值（自动钳位到 min..max 范围）
 */
void sl_progress_bar_set_value(sl_ProgressBar *bar, int value) {
    if (!bar) return;
    if (value < bar->min) value = bar->min;
    if (value > bar->max) value = bar->max;
    bar->value = value;
}
