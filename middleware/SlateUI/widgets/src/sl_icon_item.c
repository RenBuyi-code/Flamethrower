/**
 * @file    sl_icon_item.c
 * @brief   SlateUI 图标+文本组合控件实现
 *
 * 本文件实现图标与文本的水平组合控件绘制逻辑：
 *   · 根据聚焦状态选择前景色和背景色；
 *   · 图标居中绘制在上半部分，文本居中绘制在下半部分；
 *   · 选中状态在底部绘制 2px 高的选中标记；
 *   · 布局：垂直居中排列 [icon] [gap] [text]。
 */

#include "../inc/sl_icon_item.h"
#include "../../core/inc/sl_display.h"
#include "../../font/sl_font.h"
#include <string.h>

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  图标项绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 *
 * 绘制流程：
 *   1. 根据聚焦状态选择颜色方案；
 *   2. 填充背景色；
 *   3. 计算内容总高度，垂直居中；
 *   4. 绘制图标（如有），居中对齐；
 *   5. 绘制文本（如有），居中对齐；
 *   6. 若选中，在底部绘制选中标记。
 */
static void icon_item_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_IconItem *item = (sl_IconItem *)self;
    const sl_Font *font = (const sl_Font *)(item->font ? item->font : &sl_default_font);
    int focused = (self->flags & SL_WIDGET_FLAG_FOCUSED) ? 1 : 0;
    int fg_color = focused ? item->focus_fg_color : item->fg_color;
    int bg_color = focused ? item->focus_bg_color : item->bg_color;
    int has_text = (item->text && item->text[0] != '\0');
    int icon_w = (item->icon && item->icon->data) ? item->icon->width : 0;
    int icon_h = (item->icon && item->icon->data) ? item->icon->height : 0;
    int text_w = has_text ? (int)strlen(item->text) * font->width : 0;
    int content_h = icon_h;

    if (has_text) {
        if (content_h > 0) {
            content_h += item->gap;
        }
        content_h += font->height;
    }

    sl_disp_fill_rect(offset_x, offset_y, self->w, self->h, bg_color);

    if (content_h <= 0) {
        return;
    }

    int content_y = offset_y + (self->h - content_h) / 2;

    if (icon_h > 0) {
        int icon_x = offset_x + (self->w - icon_w) / 2;
        if (icon_x < offset_x + item->padding) {
            icon_x = offset_x + item->padding;
        }
        sl_disp_draw_bitmap_1bpp(icon_x, content_y, icon_w, icon_h,
                                 item->icon->data, (uint8_t)fg_color);
        content_y += icon_h + (has_text ? item->gap : 0);
    }

    if (has_text) {
        int text_x = offset_x + (self->w - text_w) / 2;
        if (text_x < offset_x + item->padding) {
            text_x = offset_x + item->padding;
        }
        sl_disp_draw_string((uint16_t)text_x, (uint16_t)content_y,
                            item->text, font, (uint8_t)fg_color);
    }

    if (item->item_flags & SL_ICON_ITEM_FLAG_SELECTED) {
        uint8_t marker_color = (uint8_t)(focused ? fg_color : item->selected_color);
        sl_disp_fill_rect(offset_x, offset_y + self->h - 2, self->w, 2, marker_color);
    }
}

/* ======================== 图标项控件接口实现 ======================== */

/**
 * @brief  初始化图标+文本组合控件
 * @param  item  指向控件实例
 * @param  x     控件左上角 X 坐标
 * @param  y     控件左上角 Y 坐标
 * @param  w     控件宽度（像素）
 * @param  h     控件高度（像素）
 * @param  icon  图标位图指针（可为 NULL）
 * @param  text  文本内容（外部持有，不拷贝）
 * @param  font  字体指针
 * @note   默认不可聚焦（由父级菜单管理焦点），
 *         普通态 fg=1/bg=0，聚焦态 fg=0/bg=1
 */
void sl_icon_item_init(sl_IconItem *item, int x, int y, int w, int h,
                       const sl_IconBitmap *icon, const char *text,
                       const void *font) {
    sl_widget_init(&item->base, x, y, w, h, icon_item_draw, NULL);
    item->icon = icon;
    item->text = text;
    item->font = font;
    item->fg_color = 1;
    item->bg_color = 0;
    item->focus_fg_color = 0;
    item->focus_bg_color = 1;
    item->selected_color = 1;
    item->padding = 2;
    item->gap = 2;
    item->item_flags = 0;
    item->base.flags &= (uint8_t)~SL_WIDGET_FLAG_FOCUSABLE;
}

/**
 * @brief  设置控件的选中状态
 * @param  item      指向控件实例
 * @param  selected  1=选中，0=取消选中
 */
void sl_icon_item_set_selected(sl_IconItem *item, uint8_t selected) {
    if (!item) {
        return;
    }

    if (selected) {
        item->item_flags |= SL_ICON_ITEM_FLAG_SELECTED;
    } else {
        item->item_flags &= (uint8_t)~SL_ICON_ITEM_FLAG_SELECTED;
    }
}
