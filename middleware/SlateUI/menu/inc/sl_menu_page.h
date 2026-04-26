/**
 * @file    sl_menu_page.h
 * @brief   SlateUI 菜单页面 —— 数据驱动菜单的 UI 层
 *
 * 本模块实现数据驱动菜单的 UI 渲染和事件处理：
 *   · 消费 sl_MenuPageModel 模型数据，自动生成列表视图界面。
 *   · 根据菜单项类型自动处理交互：
 *     - SUB_MENU:  确认后进入子菜单页面
 *     - TOGGLE:    确认后切换开关值
 *     - CHOICE:    确认后循环切换选项
 *     - ACTION:    确认后执行回调
 *     - VALUE:     确认后进入数值调整模式（上下键调整值）
 *   · 使用静态对象池分配页面实例，无需动态内存。
 *   · 页面退出时自动回收对象池槽位。
 *
 * 典型用法：
 *   extern sl_MenuPageModel main_menu_model;
 *   sl_Page *menu_page = sl_menu_page_alloc(&main_menu_model);
 *   sl_page_enter(menu_page);
 */

#ifndef SL_MENU_PAGE_H
#define SL_MENU_PAGE_H

#include "../../core/inc/sl_page.h"
#include "sl_menu_model.h"
#include "../../widgets/inc/sl_list_view.h"
#include "../../core/inc/sl_key_repeat.h"

/* ======================== 菜单页面池配置 ======================== */

/**
 * @brief  菜单页面对象池最大容量
 * @note   决定同时存在的最大菜单页面实例数（含嵌套子菜单）；
 *         可在编译选项中覆盖
 */
#ifndef SL_MENU_PAGE_POOL_SIZE
#define SL_MENU_PAGE_POOL_SIZE 4
#endif

/* ======================== 菜单页面私有数据结构体 ======================== */

/**
 * @brief  菜单页面私有数据结构体
 *
 * 存储菜单页面的运行时状态，包括模型指针、列表视图控件、
 * 按键重复器和值编辑模式标志。
 */
typedef struct {
    const sl_MenuPageModel *model;      /**< 当前菜单页模型指针（只读） */
    sl_ListView             list_view;  /**< 列表视图控件实例 */
    sl_key_repeat_t         key_repeat; /**< 按键重复生成器实例 */
    uint8_t                 editing;    /**< 值编辑模式标志（1=编辑中，0=浏览） */
} sl_MenuPageData;

/* ======================== 菜单页面接口 ======================== */

/**
 * @brief  从对象池分配一个菜单页面实例
 * @param  model  指向菜单页模型（外部持有，页面生命周期内有效）
 * @retval 指向已初始化的 sl_Page 指针，池满时返回 NULL
 * @note   分配后页面已初始化（init/draw/proc/exit 回调已设置），
 *         可直接传给 sl_page_enter() 或 sl_page_enter_with()
 */
sl_Page* sl_menu_page_alloc(const sl_MenuPageModel *model);

#endif
