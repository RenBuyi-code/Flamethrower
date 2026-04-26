/**
 * @file    sl_horizontal_menu.c
 * @brief   SlateUI 水平菜单控件实现
 *
 * 本文件实现水平滚动图标菜单的布局、绘制和事件处理：
 *   · 布局：子控件（sl_IconItem）水平排列，当前项设置 FOCUSED 标志；
 *     支持 center_active 模式（当前项居中）和普通滚动模式。
 *   · 绘制：清空背景后调用布局函数，子控件由 draw_tree 递归绘制。
 *   · 事件：左右键移动光标（支持循环），确认键选中当前项并触发回调。
 */

#include "../inc/sl_horizontal_menu.h"
#include "../../core/inc/sl_display.h"

/* ======================== 内部辅助函数 ======================== */

/**
 * @brief  统计可见子控件数量
 * @param  menu  指向菜单实例
 * @retval 可见子控件数量
 */
static int menu_get_count_internal(const sl_HorizontalMenu *menu) {
    int count = 0;
    const sl_Widget *child = menu ? menu->base.first_child : NULL;
    while (child) {
        if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
            count++;
        }
        child = child->next_sibling;
    }
    return count;
}

/**
 * @brief  根据可见索引获取子控件
 * @param  menu   指向菜单实例
 * @param  index  可见索引（0 ~ count-1）
 * @retval 指向图标项控件指针，越界返回 NULL
 */
static sl_IconItem *menu_get_item_internal(sl_HorizontalMenu *menu, int index) {
    sl_Widget *child = menu ? menu->base.first_child : NULL;
    int current = 0;
    while (child) {
        if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
            if (current == index) {
                return (sl_IconItem *)child;
            }
            current++;
        }
        child = child->next_sibling;
    }
    return NULL;
}

/**
 * @brief  整数钳位函数
 * @param  value  输入值
 * @param  min    最小值
 * @param  max    最大值
 * @retval 钳位后的值
 */
static int clamp_int(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

/**
 * @brief  重新计算菜单布局
 * @param  menu  指向菜单实例
 *
 * 布局流程：
 *   1. 遍历子控件，设置当前项的 FOCUSED 标志，计算内容总宽度；
 *   2. 根据 center_active 模式或普通滚动模式计算 scroll_x；
 *   3. 再次遍历子控件，设置每个子控件的 x/y 坐标（减去 scroll_x）。
 */
static void menu_layout(sl_HorizontalMenu *menu) {
    int content_w = 0;
    int active_center = 0;
    int active_found = 0;
    int visible_index = 0;
    sl_Widget *child = menu->base.first_child;

    while (child) {
        if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
            if (visible_index == menu->cursor) {
                child->flags |= SL_WIDGET_FLAG_FOCUSED;
                active_center = content_w + child->w / 2;
                active_found = 1;
            } else {
                child->flags &= (uint8_t)~SL_WIDGET_FLAG_FOCUSED;
            }

            content_w += child->w + menu->spacing;
            visible_index++;
        } else {
            child->flags &= (uint8_t)~SL_WIDGET_FLAG_FOCUSED;
        }
        child = child->next_sibling;
    }

    if (content_w > 0) {
        content_w -= menu->spacing;
    }

    if (!active_found || content_w <= menu->base.w) {
        menu->scroll_x = 0;
    } else if (menu->center_active) {
        menu->scroll_x = clamp_int(active_center - menu->base.w / 2,
                                   0, content_w - menu->base.w);
    } else {
        sl_IconItem *item = menu_get_item_internal(menu, menu->cursor);
        if (item) {
            if (item->base.x < menu->scroll_x) {
                menu->scroll_x = item->base.x;
            } else if (item->base.x + item->base.w > menu->scroll_x + menu->base.w) {
                menu->scroll_x = item->base.x + item->base.w - menu->base.w;
            }
            menu->scroll_x = clamp_int(menu->scroll_x, 0, content_w - menu->base.w);
        } else {
            menu->scroll_x = 0;
        }
    }

    child = menu->base.first_child;
    content_w = 0;
    while (child) {
        if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
            child->x = (int16_t)(content_w - menu->scroll_x);
            child->y = (int16_t)((menu->base.h - child->h) / 2);
            content_w += child->w + menu->spacing;
        }
        child = child->next_sibling;
    }
}

/* ======================== 内部绘制与事件回调 ======================== */

/**
 * @brief  菜单绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 * @note   清空背景后调用布局函数，子控件由 draw_tree 递归绘制
 */
static void menu_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_HorizontalMenu *menu = (sl_HorizontalMenu *)self;
    sl_disp_fill_rect(offset_x, offset_y, self->w, self->h, 0);
    menu_layout(menu);
}

/**
 * @brief  菜单事件处理回调
 * @param  self   指向控件基类
 * @param  event  指向原始输入事件
 * @retval true   事件已消费
 * @retval false  事件未消费
 *
 * 事件处理逻辑：
 *   - KEY_LEFT/UP:  光标左移，循环模式下到达开头跳到末尾
 *   - KEY_RIGHT/DOWN: 光标右移，循环模式下到达末尾跳到开头
 *   - KEY_ENTER: 选中当前项（设置 SELECTED 标志），触发 on_select 回调
 */
static bool menu_proc(sl_Widget *self, const sl_Event *event) {
    sl_HorizontalMenu *menu = (sl_HorizontalMenu *)self;
    int count = menu_get_count_internal(menu);
    int next_cursor = menu->cursor;

    if (count <= 0) {
        return false;
    }

    switch (event->type) {
    case SL_EVT_KEY_LEFT:
    case SL_EVT_KEY_UP:
        next_cursor--;
        if (next_cursor < 0) {
            next_cursor = menu->loop ? (count - 1) : 0;
        }
        menu->cursor = next_cursor;
        return true;

    case SL_EVT_KEY_RIGHT:
    case SL_EVT_KEY_DOWN:
        next_cursor++;
        if (next_cursor >= count) {
            next_cursor = menu->loop ? 0 : (count - 1);
        }
        menu->cursor = next_cursor;
        return true;

    case SL_EVT_KEY_ENTER: {
        int index = 0;
        sl_Widget *child = menu->base.first_child;
        while (child) {
            if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
                sl_icon_item_set_selected((sl_IconItem *)child,
                                          (uint8_t)(index == menu->cursor));
                index++;
            }
            child = child->next_sibling;
        }

        if (menu->on_select) {
            menu->on_select(menu, menu_get_item_internal(menu, menu->cursor), menu->cursor);
        }
        return true;
    }

    default:
        break;
    }

    return false;
}

/* ======================== 水平菜单接口实现 ======================== */

/**
 * @brief  初始化水平菜单控件
 * @param  menu     指向菜单实例
 * @param  x        控件左上角 X 坐标
 * @param  y        控件左上角 Y 坐标
 * @param  w        控件宽度（像素）
 * @param  h        控件高度（像素）
 * @param  spacing  项间距（像素）
 * @note   默认循环模式开启，居中激活开启
 */
void sl_horizontal_menu_init(sl_HorizontalMenu *menu, int x, int y, int w, int h,
                             int spacing) {
    sl_widget_init(&menu->base, x, y, w, h, menu_draw, menu_proc);
    menu->spacing = spacing;
    menu->cursor = 0;
    menu->scroll_x = 0;
    menu->loop = 1;
    menu->center_active = 1;
    menu->on_select = NULL;
    menu->user_data = NULL;
}

/**
 * @brief  向菜单添加一个图标项
 * @param  menu  指向菜单实例
 * @param  item  指向图标项控件（外部持有）
 * @note   添加后自动重新布局
 */
void sl_horizontal_menu_add_item(sl_HorizontalMenu *menu, sl_IconItem *item) {
    if (!menu || !item) {
        return;
    }

    sl_widget_add_child(&menu->base, &item->base);
    menu_layout(menu);
}

/**
 * @brief  获取菜单项数量
 * @param  menu  指向菜单实例（只读）
 * @retval 可见菜单项数量
 */
int sl_horizontal_menu_get_count(const sl_HorizontalMenu *menu) {
    return menu_get_count_internal(menu);
}

/**
 * @brief  获取当前光标索引
 * @param  menu  指向菜单实例（只读）
 * @retval 当前光标索引
 */
int sl_horizontal_menu_get_cursor(const sl_HorizontalMenu *menu) {
    return menu ? menu->cursor : 0;
}

/**
 * @brief  设置当前光标索引
 * @param  menu    指向菜单实例
 * @param  cursor  目标索引（自动钳位到有效范围）
 * @note   设置后自动重新布局
 */
void sl_horizontal_menu_set_cursor(sl_HorizontalMenu *menu, int cursor) {
    int count;
    if (!menu) {
        return;
    }

    count = menu_get_count_internal(menu);
    if (count <= 0) {
        menu->cursor = 0;
        menu->scroll_x = 0;
        return;
    }

    if (cursor < 0) {
        cursor = 0;
    } else if (cursor >= count) {
        cursor = count - 1;
    }

    menu->cursor = cursor;
    menu_layout(menu);
}

/**
 * @brief  设置菜单项选中回调
 * @param  menu       指向菜单实例
 * @param  cb         回调函数指针（可为 NULL 清除）
 * @param  user_data  传递给回调的用户数据
 */
void sl_horizontal_menu_set_on_select(sl_HorizontalMenu *menu,
                                      sl_HorizontalMenuOnSelect cb,
                                      void *user_data) {
    if (!menu) {
        return;
    }

    menu->on_select = cb;
    menu->user_data = user_data;
}

/**
 * @brief  根据索引获取菜单项
 * @param  menu   指向菜单实例
 * @param  index  项索引（0 ~ count-1）
 * @retval 指向图标项控件指针，越界返回 NULL
 */
sl_IconItem *sl_horizontal_menu_get_item(sl_HorizontalMenu *menu, int index) {
    return menu_get_item_internal(menu, index);
}
