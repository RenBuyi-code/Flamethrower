/**
 * @file    sl_horizontal_menu.h
 * @brief   SlateUI 水平菜单控件
 *
 * 本模块实现水平滚动的图标菜单控件，常用于底部导航栏：
 *   · 子控件为 sl_IconItem，水平排列，支持左右键导航。
 *   · 支持循环模式（loop），到达末尾后回到开头。
 *   · 支持居中激活模式（center_active），当前项始终居中显示。
 *   · 确认键触发 on_select 回调，通知应用层选中了哪一项。
 *
 * 典型用法：
 *   sl_HorizontalMenu menu;
 *   sl_horizontal_menu_init(&menu, 0, 54, 128, 10, 4);
 *   sl_horizontal_menu_add_item(&menu, &item1);
 *   sl_horizontal_menu_set_on_select(&menu, on_select, NULL);
 */

#ifndef SL_HORIZONTAL_MENU_H
#define SL_HORIZONTAL_MENU_H

#include "sl_widget.h"
#include "sl_icon_item.h"

/* ======================== 前向声明与回调类型 ======================== */

/** @brief 水平菜单结构体前向声明 */
struct sl_HorizontalMenu;

/**
 * @brief  菜单项选中回调函数类型
 * @param  menu   指向菜单控件实例
 * @param  item   指向被选中的图标项控件
 * @param  index  被选中项的索引
 */
typedef void (*sl_HorizontalMenuOnSelect)(struct sl_HorizontalMenu *menu,
                                          sl_IconItem *item, int index);

/* ======================== 水平菜单控件结构体 ======================== */

/**
 * @brief  水平菜单控件结构体
 *
 * 继承 sl_Widget 基类，增加导航状态、循环模式和选中回调等属性。
 * 子控件通过 sl_widget_add_child() 添加为 sl_IconItem。
 */
typedef struct sl_HorizontalMenu {
    sl_Widget               base;           /**< 控件基类（必须为第一个成员） */
    int                     spacing;        /**< 项间距（像素） */
    int                     cursor;         /**< 当前光标索引（0 ~ count-1） */
    int                     scroll_x;       /**< 水平滚动偏移（像素） */
    uint8_t                 loop;           /**< 是否循环导航（1=循环，0=不循环） */
    uint8_t                 center_active;  /**< 是否居中激活（1=当前项居中） */
    sl_HorizontalMenuOnSelect on_select;    /**< 选中回调函数指针 */
    void                   *user_data;      /**< 用户自定义数据，传递给 on_select */
} sl_HorizontalMenu;

/* ======================== 水平菜单接口 ======================== */

/**
 * @brief  初始化水平菜单控件
 * @param  menu     指向菜单实例
 * @param  x        控件左上角 X 坐标
 * @param  y        控件左上角 Y 坐标
 * @param  w        控件宽度（像素）
 * @param  h        控件高度（像素）
 * @param  spacing  项间距（像素）
 * @note   初始化后光标为 0，不循环，不居中激活
 */
void sl_horizontal_menu_init(sl_HorizontalMenu *menu, int x, int y, int w, int h,
                             int spacing);

/**
 * @brief  向菜单添加一个图标项
 * @param  menu  指向菜单实例
 * @param  item  指向图标项控件（外部持有）
 * @note   添加顺序即为显示顺序
 */
void sl_horizontal_menu_add_item(sl_HorizontalMenu *menu, sl_IconItem *item);

/**
 * @brief  获取菜单项数量
 * @param  menu  指向菜单实例（只读）
 * @retval 菜单项数量
 */
int  sl_horizontal_menu_get_count(const sl_HorizontalMenu *menu);

/**
 * @brief  获取当前光标索引
 * @param  menu  指向菜单实例（只读）
 * @retval 当前光标索引（0 ~ count-1）
 */
int  sl_horizontal_menu_get_cursor(const sl_HorizontalMenu *menu);

/**
 * @brief  设置当前光标索引
 * @param  menu    指向菜单实例
 * @param  cursor  目标索引（自动钳位到有效范围）
 */
void sl_horizontal_menu_set_cursor(sl_HorizontalMenu *menu, int cursor);

/**
 * @brief  设置菜单项选中回调
 * @param  menu       指向菜单实例
 * @param  cb         回调函数指针（可为 NULL 清除）
 * @param  user_data  传递给回调的用户数据
 */
void sl_horizontal_menu_set_on_select(sl_HorizontalMenu *menu,
                                      sl_HorizontalMenuOnSelect cb,
                                      void *user_data);

/**
 * @brief  根据索引获取菜单项
 * @param  menu   指向菜单实例
 * @param  index  项索引（0 ~ count-1）
 * @retval 指向图标项控件指针，越界返回 NULL
 */
sl_IconItem *sl_horizontal_menu_get_item(sl_HorizontalMenu *menu, int index);

#endif
