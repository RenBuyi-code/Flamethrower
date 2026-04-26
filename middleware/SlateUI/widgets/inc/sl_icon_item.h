/**
 * @file    sl_icon_item.h
 * @brief   SlateUI 图标+文本组合控件
 *
 * 本模块实现图标与文本的水平组合控件，常用于菜单项：
 *   · 左侧显示 1bpp 位图图标，右侧显示文本标签。
 *   · 支持普通态、聚焦态和选中态三种视觉状态，
 *     每种状态有独立的前景色和背景色。
 *   · 选中态通过 SL_ICON_ITEM_FLAG_SELECTED 标志控制，
 *     通常用于多选列表中的勾选标记。
 */

#ifndef SL_ICON_ITEM_H
#define SL_ICON_ITEM_H

#include "sl_widget.h"
#include "sl_icon.h"

/* ======================== 图标项控件结构体 ======================== */

/**
 * @brief  图标+文本组合控件结构体
 *
 * 继承 sl_Widget 基类，增加图标、文本、颜色和间距等属性。
 * 布局：[padding] [icon] [gap] [text] [padding]
 */
typedef struct {
    sl_Widget          base;            /**< 控件基类（必须为第一个成员） */
    const sl_IconBitmap *icon;          /**< 图标位图指针（可为 NULL） */
    const char        *text;            /**< 文本内容（外部持有，不拷贝） */
    const void        *font;            /**< 字体指针 */
    uint8_t            fg_color;        /**< 普通态前景色 (0=灭, 非0=亮) */
    uint8_t            bg_color;        /**< 普通态背景色 (0=灭, 非0=亮) */
    uint8_t            focus_fg_color;  /**< 聚焦态前景色 */
    uint8_t            focus_bg_color;  /**< 聚焦态背景色 */
    uint8_t            selected_color;  /**< 选中标记颜色 */
    uint8_t            padding;         /**< 内边距（像素） */
    uint8_t            gap;             /**< 图标与文本间距（像素） */
    uint8_t            item_flags;      /**< 项标志位 (SL_ICON_ITEM_FLAG_xxx) */
} sl_IconItem;

/* ======================== 项标志位定义 ======================== */

/** @brief 选中标志（用于多选列表的勾选标记） */
#define SL_ICON_ITEM_FLAG_SELECTED 0x01

/* ======================== 图标项控件接口 ======================== */

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
 * @note   默认普通态颜色（fg=1, bg=0），聚焦态颜色（fg=0, bg=1）
 */
void sl_icon_item_init(sl_IconItem *item, int x, int y, int w, int h,
                       const sl_IconBitmap *icon, const char *text,
                       const void *font);

/**
 * @brief  设置控件的选中状态
 * @param  item      指向控件实例
 * @param  selected  1=选中，0=取消选中
 * @note   选中后绘制时在左侧显示选中标记
 */
void sl_icon_item_set_selected(sl_IconItem *item, uint8_t selected);

#endif
