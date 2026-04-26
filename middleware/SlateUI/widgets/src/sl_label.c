/**
 * @file    sl_label.c
 * @brief   SlateUI 文本标签控件实现
 *
 * 本文件实现文本标签的绘制和自动滚动逻辑：
 *   · 绘制时根据对齐方式计算起始 X 坐标，垂直方向居中。
 *   · 自动滚动模式下，文本超出控件宽度时启动跑马灯效果：
 *     先暂停 SL_LABEL_SCROLL_PAUSE_FRAMES 帧，然后向左滚动，
 *     滚动到末尾后重置并再次暂停。
 *   · 滚动期间在文本末尾追加一段间距后重复绘制文本，
 *     实现无缝循环视觉效果。
 */

#include "../inc/sl_label.h"
#include "../../font/sl_font.h"
#include "../../core/inc/sl_display.h"
#include <string.h>

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  标签绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 *
 * 绘制逻辑：
 *   1. 若文本为空或控件不可见，直接返回；
 *   2. 计算文本像素宽度，垂直居中；
 *   3. 自动滚动模式：根据 scroll_offset 偏移绘制文本，
 *      并在末尾间距后追加一次文本实现无缝循环；
 *   4. 非滚动模式：根据对齐方式计算起始 X 坐标后绘制。
 */
static void label_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_Label *label = (sl_Label *)self;

    if (!label->text || !(self->flags & SL_WIDGET_FLAG_VISIBLE)) {
        return;
    }

    const sl_Font *font = (const sl_Font *)(label->font ? label->font : &sl_default_font);
    int text_w = (int)strlen(label->text) * font->width;
    int y_center = offset_y + (self->h - font->height) / 2;

    if (label->scroll == SL_LABEL_SCROLL_AUTO && text_w > self->w) {
        int x = offset_x - label->scroll_offset;
        sl_disp_draw_string(x, y_center, label->text, font, label->color);

        int overflow = text_w - self->w;
        if (label->scroll_offset > overflow + font->width * 4) {
            int wrap_x = offset_x - label->scroll_offset + text_w + font->width * 4;
            sl_disp_draw_string(wrap_x, y_center, label->text, font, label->color);
        }
    } else {
        int x_start = offset_x;
        if (label->align == SL_LABEL_ALIGN_CENTER) {
            x_start += (self->w - text_w) / 2;
            if (x_start < offset_x) x_start = offset_x;
        } else if (label->align == SL_LABEL_ALIGN_RIGHT) {
            x_start += self->w - text_w;
        }
        sl_disp_draw_string(x_start, y_center, label->text, font, label->color);
    }
}

/* ======================== 标签控件接口实现 ======================== */

/**
 * @brief  初始化文本标签控件
 * @param  label  指向标签控件实例
 * @param  x      控件左上角 X 坐标
 * @param  y      控件左上角 Y 坐标
 * @param  w      控件宽度（像素）
 * @param  h      控件高度（像素）
 * @param  text   文本内容指针（外部持有，不拷贝）
 * @param  font   字体指针（NULL 使用默认字体）
 * @param  color  文字颜色 (0=灭, 非0=亮)
 * @param  align  水平对齐方式 (SL_LABEL_ALIGN_xxx)
 * @note   默认不滚动，需调用 sl_label_set_scroll() 启用
 */
void sl_label_init(sl_Label *label, int x, int y, int w, int h,
                   const char *text, const void *font,
                   uint8_t color, uint8_t align) {
    sl_widget_init(&label->base, x, y, w, h, label_draw, NULL);

    label->text  = text;
    label->font  = font;
    label->color = color;
    label->align = align;
    label->scroll = SL_LABEL_SCROLL_NONE;
    label->scroll_offset = 0;
    label->scroll_pause  = 0;
}

/**
 * @brief  设置标签滚动模式
 * @param  label  指向标签控件实例
 * @param  mode   滚动模式 (SL_LABEL_SCROLL_NONE / SL_LABEL_SCROLL_AUTO)
 * @note   启用自动滚动时重置偏移并设置初始暂停帧数
 */
void sl_label_set_scroll(sl_Label *label, uint8_t mode) {
    if (!label) return;
    label->scroll = mode;
    label->scroll_offset = 0;
    label->scroll_pause  = SL_LABEL_SCROLL_PAUSE_FRAMES;
}

/**
 * @brief  标签控件时钟节拍
 * @param  label  指向标签控件实例
 *
 * 滚动驱动逻辑：
 *   1. 若非自动滚动或文本未超出控件宽度，直接返回；
 *   2. 暂停帧数 > 0 时递减，不滚动；
 *   3. 每帧增加 SL_LABEL_SCROLL_SPEED 像素偏移；
 *   4. 滚动到末尾后重置偏移并重新暂停。
 */
void sl_label_tick(sl_Label *label) {
    if (!label || label->scroll != SL_LABEL_SCROLL_AUTO) return;
    if (!label->text) return;

    const sl_Font *font = (const sl_Font *)(label->font ? label->font : &sl_default_font);
    int text_w = (int)strlen(label->text) * font->width;

    if (text_w <= label->base.w) return;

    if (label->scroll_pause > 0) {
        label->scroll_pause--;
        return;
    }

    label->scroll_offset += SL_LABEL_SCROLL_SPEED;

    int overflow = text_w - label->base.w;
    if (label->scroll_offset > overflow + font->width * 4 + SL_LABEL_SCROLL_SPEED) {
        label->scroll_offset = 0;
        label->scroll_pause = SL_LABEL_SCROLL_PAUSE_FRAMES;
    }
}
