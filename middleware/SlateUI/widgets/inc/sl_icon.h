/**
 * @file    sl_icon.h
 * @brief   SlateUI 图标控件
 *
 * 本模块实现 1bpp 位图图标控件，支持水平和垂直对齐：
 *   · 显示 sl_IconBitmap 格式的单色位图。
 *   · 支持水平对齐（左/中/右）和垂直对齐（上/中/下）。
 *   · 可选背景填充（SL_ICON_FLAG_FILL_BG），在位图区域外填充背景色。
 *   · 位图数据由外部持有，控件不拷贝。
 */

#ifndef SL_ICON_H
#define SL_ICON_H

#include "sl_widget.h"

/* ======================== 位图数据结构 ======================== */

/**
 * @brief  1bpp 单色位图数据结构
 *
 * 描述一个单色位图的尺寸和数据指针：
 *   · data 指向位图数据，每行 ceil(width/8) 字节，MSB 在左。
 *   · 与 sl_disp_draw_bitmap_1bpp() 的格式一致。
 */
typedef struct {
    const uint8_t *data;    /**< 位图数据指针（外部持有，Flash 或 RAM） */
    uint8_t        width;   /**< 位图宽度（像素） */
    uint8_t        height;  /**< 位图高度（像素） */
} sl_IconBitmap;

/* ======================== 图标控件结构体 ======================== */

/**
 * @brief  图标控件结构体
 *
 * 继承 sl_Widget 基类，增加位图、颜色和对齐方式等属性。
 */
typedef struct {
    sl_Widget          base;       /**< 控件基类（必须为第一个成员） */
    const sl_IconBitmap *bitmap;   /**< 当前显示的位图指针（可为 NULL） */
    uint8_t            color;      /**< 前景色 (0=灭, 非0=亮) */
    uint8_t            bg_color;   /**< 背景色 (0=灭, 非0=亮) */
    uint8_t            flags;      /**< 图标标志位 (SL_ICON_FLAG_xxx) */
    uint8_t            align;      /**< 水平对齐方式 (SL_ICON_ALIGN_xxx) */
    uint8_t            valign;     /**< 垂直对齐方式 (SL_ICON_VALIGN_xxx) */
} sl_Icon;

/* ======================== 对齐方式定义 ======================== */

/** @brief 水平左对齐 */
#define SL_ICON_ALIGN_LEFT      0

/** @brief 水平居中对齐 */
#define SL_ICON_ALIGN_CENTER    1

/** @brief 水平右对齐 */
#define SL_ICON_ALIGN_RIGHT     2

/** @brief 垂直顶部对齐 */
#define SL_ICON_VALIGN_TOP      0

/** @brief 垂直居中对齐 */
#define SL_ICON_VALIGN_CENTER   1

/** @brief 垂直底部对齐 */
#define SL_ICON_VALIGN_BOTTOM   2

/* ======================== 图标标志位定义 ======================== */

/** @brief 填充背景标志（在位图区域外填充 bg_color） */
#define SL_ICON_FLAG_FILL_BG    0x01

/* ======================== 图标控件接口 ======================== */

/**
 * @brief  初始化图标控件
 * @param  icon    指向图标控件实例
 * @param  x       控件左上角 X 坐标
 * @param  y       控件左上角 Y 坐标
 * @param  w       控件宽度（像素）
 * @param  h       控件高度（像素）
 * @param  bitmap  位图数据指针（可为 NULL，后续通过 set_bitmap 设置）
 * @param  color   前景色 (0=灭, 非0=亮)
 * @note   默认左上角对齐，不填充背景
 */
void sl_icon_init(sl_Icon *icon, int x, int y, int w, int h,
                  const sl_IconBitmap *bitmap, uint8_t color);

/**
 * @brief  设置图标显示的位图
 * @param  icon    指向图标控件实例
 * @param  bitmap  新的位图数据指针（可为 NULL 表示清空）
 * @note   切换位图后需手动调用 sl_page_request_redraw() 刷新
 */
void sl_icon_set_bitmap(sl_Icon *icon, const sl_IconBitmap *bitmap);

#endif
