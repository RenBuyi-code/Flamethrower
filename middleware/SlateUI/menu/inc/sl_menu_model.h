/**
 * @file    sl_menu_model.h
 * @brief   SlateUI 数据驱动菜单模型
 *
 * 本模块定义菜单的数据模型，实现 UI 状态与业务逻辑的分离：
 *   · 菜单项 (sl_MenuItem) 描述单个菜单条目的类型、文本和关联数据。
 *   · 菜单页 (sl_MenuPageModel) 描述一组菜单项的集合，包含标题和项数组。
 *   · 菜单项类型 (sl_MenuItemType) 区分不同交互模式：
 *     - SUB_MENU:  子菜单，进入下一级页面
 *     - TOGGLE:    开关项，点击切换布尔值
 *     - CHOICE:    多选一，点击循环切换选项
 *     - ACTION:    动作项，点击执行回调
 *     - VALUE:     数值项，点击进入数值调整页面
 *   · 模型仅描述数据，不包含任何 UI 绘制逻辑，
 *     由 sl_menu_page 消费模型并渲染界面。
 *
 * 设计理念：
 *   类似 Android RecyclerView.Adapter，模型提供数据，
 *   页面负责展示，两者通过 sl_MenuPageModel 指针连接。
 *   同一个模型可被多个页面实例共享（只读）。
 */

#ifndef SL_MENU_MODEL_H
#define SL_MENU_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/* ======================== 菜单项类型枚举 ======================== */

/**
 * @brief  菜单项类型枚举
 *
 * 定义菜单项的交互模式，决定点击后的行为。
 */
typedef enum {
    SL_MENU_SUB_MENU = 0,  /**< 子菜单项，点击进入下一级页面 */
    SL_MENU_TOGGLE,        /**< 开关项，点击切换布尔值（开/关） */
    SL_MENU_CHOICE,        /**< 多选一项，点击循环切换选项索引 */
    SL_MENU_ACTION,        /**< 动作项，点击执行 on_action 回调 */
    SL_MENU_VALUE          /**< 数值项，点击进入数值调整页面 */
} sl_MenuItemType;

/* ======================== 前向声明与回调类型 ======================== */

/** @brief 菜单项结构体前向声明 */
struct sl_MenuItem;

/**
 * @brief  菜单动作回调函数类型
 * @param  item  指向触发动作的菜单项
 * @note   由 SL_MENU_ACTION 类型项在确认时调用
 */
typedef void (*sl_MenuActionCb)(const struct sl_MenuItem *item);

/**
 * @brief  菜单项值读取回调函数类型
 * @param  item  指向菜单项（只读）
 * @retval 当前值
 * @note   当业务层需要作为 value 的唯一真相源时设置此回调；
 *         NULL 表示直接读取 item->value（默认行为）
 */
typedef int16_t (*sl_MenuValueGetter)(const struct sl_MenuItem *item);

/**
 * @brief  菜单项值写入回调函数类型
 * @param  item   指向菜单项
 * @param  value  目标值
 * @note   当业务层需要拦截/校验/转发 value 变更时设置此回调；
 *         NULL 表示直接写入 item->value（默认行为）
 *         回调内可执行硬件操作、EEPROM 写入、范围校验等
 */
typedef void (*sl_MenuValueSetter)(struct sl_MenuItem *item, int16_t value);

/* ======================== 菜单项结构体 ======================== */

/**
 * @brief  菜单项结构体
 *
 * 描述单个菜单条目的所有属性，包括类型、文本、
 * 当前值、可选范围和动作回调。
 */
typedef struct sl_MenuItem {
    const char       *text;       /**< 菜单项显示文本（外部持有，不拷贝） */
    sl_MenuItemType   type;       /**< 菜单项类型 */
    int16_t           value;      /**< 当前值（TOGGLE: 0/1, CHOICE: 选项索引, VALUE: 数值） */
    int16_t           min;        /**< 最小值（VALUE 类型使用） */
    int16_t           max;        /**< 最大值（VALUE 类型使用） */
    const char      **choices;    /**< 选项文本数组（CHOICE 类型使用，外部持有） */
    int16_t           choice_count;/**< 选项数量（CHOICE 类型使用） */
    sl_MenuActionCb   on_action;  /**< 动作回调（ACTION 类型使用） */
    struct sl_MenuPageModel *sub; /**< 子菜单模型指针（SUB_MENU 类型使用） */
    sl_MenuValueGetter  get_value;/**< 可选：外部值读取回调（NULL=直接读 item->value） */
    sl_MenuValueSetter  set_value;/**< 可选：外部值写入回调（NULL=直接写 item->value） */
} sl_MenuItem;

/* ======================== 菜单页模型结构体 ======================== */

/**
 * @brief  菜单页模型结构体
 *
 * 描述一组菜单项的集合，包含标题和项数组。
 * 模型为只读数据，由页面实例消费。
 */
typedef struct sl_MenuPageModel {
    const char       *title;      /**< 页面标题文本（外部持有） */
    const sl_MenuItem *items;     /**< 菜单项数组指针（外部持有） */
    int16_t           item_count; /**< 菜单项数量 */
} sl_MenuPageModel;

/* ======================== 菜单模型操作接口 ======================== */

/**
 * @brief  切换 TOGGLE 类型菜单项的布尔值
 * @param  item  指向菜单项
 * @note   value 在 0 和 1 之间切换；
 *         非 TOGGLE 类型项调用无效
 */
void sl_menu_item_toggle(sl_MenuItem *item);

/**
 * @brief  切换 CHOICE 类型菜单项的选项索引
 * @param  item  指向菜单项
 * @note   value 递增并循环回 0；
 *         非 CHOICE 类型项调用无效
 */
void sl_menu_item_next_choice(sl_MenuItem *item);

/**
 * @brief  获取 CHOICE 类型菜单项的当前选项文本
 * @param  item  指向菜单项（只读）
 * @retval 当前选项文本指针，非 CHOICE 类型或越界返回 NULL
 */
const char* sl_menu_item_get_choice_text(const sl_MenuItem *item);

/**
 * @brief  获取 TOGGLE 类型菜单项的显示文本（开/关）
 * @param  item  指向菜单项（只读）
 * @retval "ON" 或 "OFF" 字符串指针
 * @note   非 TOGGLE 类型返回 "OFF"
 */
const char* sl_menu_item_get_toggle_text(const sl_MenuItem *item);

/**
 * @brief  读取菜单项的当前值
 * @param  item  指向菜单项
 * @retval 当前 value 值
 * @note   若 item->get_value 不为 NULL，调用外部回调读取；
 *         否则直接返回 item->value（默认行为）
 */
int16_t sl_menu_item_get_value(const sl_MenuItem *item);

/**
 * @brief  写入菜单项的当前值
 * @param  item   指向菜单项
 * @param  value  目标值
 * @note   若 item->set_value 不为 NULL，调用外部回调写入
 *         （业务层可在回调中执行校验、硬件操作等）；
 *         否则直接写入 item->value（默认行为）
 */
void sl_menu_item_set_value(sl_MenuItem *item, int16_t value);

#endif
