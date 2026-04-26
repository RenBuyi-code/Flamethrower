/**
 * @file    sl_list_view.h
 * @brief   SlateUI 列表视图控件
 *
 * 本模块实现垂直滚动列表控件，支持光标动画和滚动条：
 *   · 垂直滚动列表，每项固定高度，支持上下键导航。
 *   · 光标移动时使用 sl_tween_t 插值动画，平滑过渡。
 *   · 可选滚动条指示器，显示当前视口在列表中的位置。
 *   · 焦点变化时自动投递 SL_UI_EVT_FOCUS_CHANGED 语义事件。
 *   · 确认键投递 SL_UI_EVT_ENTER_ITEM 语义事件。
 *
 * 典型用法：
 *   sl_ListView lv;
 *   sl_list_view_init(&lv, 0, 10, 128, 5, 12, font);
 *   sl_list_view_set_items(&lv, items, count);
 *   sl_list_view_set_scrollbar(&lv, 1);
 *   // 主循环中：
 *   sl_list_view_tick(&lv, delta_ms);
 */

#ifndef SL_LIST_VIEW_H
#define SL_LIST_VIEW_H

#include "sl_widget.h"
#include "../../core/inc/sl_tween.h"

/* ======================== 列表项数据结构 ======================== */

/**
 * @brief  列表项数据结构
 *
 * 每个列表项包含一个文本指针，由外部持有，不拷贝。
 */
typedef struct {
    const char *text;   /**< 列表项文本（外部持有，不拷贝） */
} sl_ListItem;

/**
 * @brief  光标动画默认时长（毫秒）
 * @note   可在编译选项中覆盖，设为 0 则禁用光标动画
 */
#ifndef SL_LIST_CURSOR_ANIM_MS
#define SL_LIST_CURSOR_ANIM_MS 120
#endif

/* ======================== 列表视图控件结构体 ======================== */

/**
 * @brief  列表视图控件结构体
 *
 * 继承 sl_Widget 基类，增加列表数据、滚动状态、
 * 光标位置和动画插值器等属性。
 */
typedef struct {
    sl_Widget         base;           /**< 控件基类（必须为第一个成员） */
    const sl_ListItem *items;         /**< 列表项数组指针（外部持有） */
    int               item_count;     /**< 列表项总数 */
    int               item_height;    /**< 每项高度（像素） */
    int               visible_count;  /**< 可见项数量（控件高度 / item_height） */
    int               scroll_offset;  /**< 垂直滚动偏移（像素，正值向下） */
    int               cursor;         /**< 当前光标索引（0 ~ item_count-1） */
    const void       *font;           /**< 字体指针 */
    sl_tween_t        cursor_tween;   /**< 光标 Y 坐标插值动画器 */
    uint8_t           show_scrollbar; /**< 是否显示滚动条（1=显示，0=隐藏） */
} sl_ListView;

/* ======================== 列表视图接口 ======================== */

/**
 * @brief  初始化列表视图控件
 * @param  lv             指向列表视图实例
 * @param  x              控件左上角 X 坐标
 * @param  y              控件左上角 Y 坐标
 * @param  w              控件宽度（像素）
 * @param  visible_count  可见项数量
 * @param  item_height    每项高度（像素）
 * @param  font           字体指针
 * @note   初始化后光标位于第一项，无列表数据
 */
void sl_list_view_init(sl_ListView *lv, int x, int y, int w,
                       int visible_count, int item_height,
                       const void *font);

/**
 * @brief  设置列表项数据
 * @param  lv     指向列表视图实例
 * @param  items  列表项数组指针（外部持有，不拷贝）
 * @param  count  列表项数量
 * @note   调用后光标重置为 0，滚动偏移重置
 */
void sl_list_view_set_items(sl_ListView *lv, const sl_ListItem *items, int count);

/**
 * @brief  设置是否显示滚动条
 * @param  lv    指向列表视图实例
 * @param  show  1=显示滚动条，0=隐藏
 * @note   滚动条绘制在控件右侧 2 像素宽的区域
 */
void sl_list_view_set_scrollbar(sl_ListView *lv, uint8_t show);

/**
 * @brief  列表视图时钟节拍
 * @param  lv        指向列表视图实例
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   推进光标动画插值器，需在主循环中周期性调用
 */
void sl_list_view_tick(sl_ListView *lv, uint16_t delta_ms);

#endif
