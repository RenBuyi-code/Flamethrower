/**
 * @file    sl_icon.c
 * @brief   SlateUI 图标控件实现
 *
 * 本文件实现 1bpp 位图图标的绘制逻辑：
 *   · 可选背景填充（SL_ICON_FLAG_FILL_BG）；
 *   · 根据水平/垂直对齐方式计算位图绘制位置；
 *   · 位图数据通过 sl_disp_draw_bitmap_1bpp() 绘制。
 */

#include "../inc/sl_icon.h"
#include "../../core/inc/sl_display.h"

/* ======================== 内部对齐计算函数 ======================== */

/**
 * @brief  计算水平对齐偏移量
 * @param  container  容器宽度（像素）
 * @param  content    内容宽度（像素）
 * @param  align      对齐方式 (SL_ICON_ALIGN_xxx)
 * @retval 内容在容器中的 X 偏移量
 */
static int align_offset(int container, int content, uint8_t align) {
    if (content >= container) {
        return 0;
    }

    if (align == SL_ICON_ALIGN_CENTER) {
        return (container - content) / 2;
    }
    if (align == SL_ICON_ALIGN_RIGHT) {
        return container - content;
    }
    return 0;
}

/**
 * @brief  计算垂直对齐偏移量
 * @param  container  容器高度（像素）
 * @param  content    内容高度（像素）
 * @param  valign     垂直对齐方式 (SL_ICON_VALIGN_xxx)
 * @retval 内容在容器中的 Y 偏移量
 */
static int valign_offset(int container, int content, uint8_t valign) {
    if (content >= container) {
        return 0;
    }

    if (valign == SL_ICON_VALIGN_CENTER) {
        return (container - content) / 2;
    }
    if (valign == SL_ICON_VALIGN_BOTTOM) {
        return container - content;
    }
    return 0;
}

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  图标绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 *
 * 绘制流程：
 *   1. 若位图数据为空，直接返回；
 *   2. 若设置了 FILL_BG 标志，先填充背景色；
 *   3. 根据对齐方式计算位图位置，绘制 1bpp 位图。
 */
static void icon_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_Icon *icon = (sl_Icon *)self;
    if (!icon->bitmap || !icon->bitmap->data) {
        return;
    }

    if (icon->flags & SL_ICON_FLAG_FILL_BG) {
        sl_disp_fill_rect(offset_x, offset_y, self->w, self->h, icon->bg_color);
    }

    sl_disp_draw_bitmap_1bpp(
        offset_x + align_offset(self->w, icon->bitmap->width, icon->align),
        offset_y + valign_offset(self->h, icon->bitmap->height, icon->valign),
        icon->bitmap->width, icon->bitmap->height,
        icon->bitmap->data, icon->color);
}

/* ======================== 图标控件接口实现 ======================== */

/**
 * @brief  初始化图标控件
 * @param  icon    指向图标控件实例
 * @param  x       控件左上角 X 坐标
 * @param  y       控件左上角 Y 坐标
 * @param  w       控件宽度（像素）
 * @param  h       控件高度（像素）
 * @param  bitmap  位图数据指针（可为 NULL）
 * @param  color   前景色 (0=灭, 非0=亮)
 * @note   默认居中对齐，不填充背景
 */
void sl_icon_init(sl_Icon *icon, int x, int y, int w, int h,
                  const sl_IconBitmap *bitmap, uint8_t color) {
    sl_widget_init(&icon->base, x, y, w, h, icon_draw, NULL);
    icon->bitmap = bitmap;
    icon->color = color;
    icon->bg_color = 0;
    icon->flags = 0;
    icon->align = SL_ICON_ALIGN_CENTER;
    icon->valign = SL_ICON_VALIGN_CENTER;
}

/**
 * @brief  设置图标显示的位图
 * @param  icon    指向图标控件实例
 * @param  bitmap  新的位图数据指针（可为 NULL）
 */
void sl_icon_set_bitmap(sl_Icon *icon, const sl_IconBitmap *bitmap) {
    if (!icon) {
        return;
    }
    icon->bitmap = bitmap;
}
