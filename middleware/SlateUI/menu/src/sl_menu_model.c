/**
 * @file    sl_menu_model.c
 * @brief   SlateUI 数据驱动菜单模型实现
 *
 * 本文件实现菜单项的值操作函数：
 *   · TOGGLE 切换、CHOICE 循环切换、获取显示文本等。
 *   · 所有操作均为 O(1)，无动态内存分配。
 */

#include "../inc/sl_menu_model.h"

/**
 * @brief  切换 TOGGLE 类型菜单项的布尔值
 * @param  item  指向菜单项
 * @note   value 在 0 和 1 之间取反；
 *         非 TOGGLE 类型项不做任何操作；
 *         通过 sl_menu_item_set_value 写入，支持外部回调拦截
 */
void sl_menu_item_toggle(sl_MenuItem *item) {
    if (!item || item->type != SL_MENU_TOGGLE) {
        return;
    }

    sl_menu_item_set_value(item, sl_menu_item_get_value(item) ? (int16_t)0 : (int16_t)1);
}

/**
 * @brief  切换 CHOICE 类型菜单项的选项索引
 * @param  item  指向菜单项
 * @note   value 递增 1，到达 choice_count 后循环回 0；
 *         非 CHOICE 类型项不做任何操作；
 *         通过 sl_menu_item_set_value 写入，支持外部回调拦截
 */
void sl_menu_item_next_choice(sl_MenuItem *item) {
    if (!item || item->type != SL_MENU_CHOICE) {
        return;
    }

    int16_t next = (int16_t)(sl_menu_item_get_value(item) + 1);
    if (next >= item->choice_count) {
        next = 0;
    }
    sl_menu_item_set_value(item, next);
}

/**
 * @brief  获取 CHOICE 类型菜单项的当前选项文本
 * @param  item  指向菜单项（只读）
 * @retval 当前选项文本指针
 * @note   若 choices 为 NULL 或 value 越界，返回空字符串 ""
 */
const char* sl_menu_item_get_choice_text(const sl_MenuItem *item) {
    if (!item || item->type != SL_MENU_CHOICE) {
        return "";
    }

    if (!item->choices || sl_menu_item_get_value(item) < 0 || sl_menu_item_get_value(item) >= item->choice_count) {
        return "";
    }

    return item->choices[sl_menu_item_get_value(item)];
}

/**
 * @brief  获取 TOGGLE 类型菜单项的显示文本
 * @param  item  指向菜单项（只读）
 * @retval "ON"（value=1）或 "OFF"（value=0 或非 TOGGLE 类型）
 */
const char* sl_menu_item_get_toggle_text(const sl_MenuItem *item) {
    if (!item || item->type != SL_MENU_TOGGLE) {
        return "OFF";
    }

    return sl_menu_item_get_value(item) ? "ON" : "OFF";
}

/**
 * @brief  读取菜单项的当前值
 * @param  item  指向菜单项
 * @retval 当前 value 值
 */
int16_t sl_menu_item_get_value(const sl_MenuItem *item) {
    if (!item) {
        return 0;
    }
    if (item->get_value) {
        return item->get_value(item);
    }
    return item->value;
}

/**
 * @brief  写入菜单项的当前值
 * @param  item   指向菜单项
 * @param  value  目标值
 */
void sl_menu_item_set_value(sl_MenuItem *item, int16_t value) {
    if (!item) {
        return;
    }
    if (item->set_value) {
        item->set_value(item, value);
    } else {
        item->value = value;
    }
}
