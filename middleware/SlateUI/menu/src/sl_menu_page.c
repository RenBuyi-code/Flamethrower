/**
 * @file    sl_menu_page.c
 * @brief   SlateUI 菜单页面实现 —— 数据驱动菜单的 UI 层
 *
 * 本文件实现菜单页面的生命周期回调和事件处理：
 *   · init:    初始化列表视图、按键重复器，订阅 UI 语义事件
 *   · draw:    绘制标题栏和列表视图
 *   · proc:    分发事件给列表视图和按键重复器，处理返回键
 *   · exit:    取消订阅 UI 语义事件
 *   · presenter: 处理 UI 语义事件（焦点变化、确认选中项）
 *
 * 菜单项交互逻辑（在 presenter 中）：
 *   - SUB_MENU:  确认后递归分配子菜单页面并推入栈
 *   - TOGGLE:    确认后切换开关值
 *   - CHOICE:    确认后循环切换选项
 *   - ACTION:    确认后执行回调
 *   - VALUE:     确认后进入值编辑模式（上下键调整值，再次确认退出编辑）
 *
 * 对象池管理：
 *   · 使用静态数组 menu_page_pool 分配页面实例
 *   · pool_used 位图跟踪槽位占用状态
 *   · 退出时释放槽位，支持页面嵌套和返回
 */

#include "../inc/sl_menu_page.h"
#include "../../core/inc/sl_display.h"
#include "../../core/inc/sl_page_manager.h"
#include "../../core/inc/sl_language.h"
#include "../../font/sl_font.h"
#include <string.h>

/* ======================== 菜单页面对象池 ======================== */

/** @brief 菜单页面实例静态池 */
static sl_Page menu_page_pool[SL_MENU_PAGE_POOL_SIZE];

/** @brief 菜单页面私有数据静态池 */
static sl_MenuPageData menu_page_data_pool[SL_MENU_PAGE_POOL_SIZE];

/** @brief 槽位占用位图（bit=1 表示已占用） */
static uint8_t pool_used = 0;

/* ======================== 内部辅助函数 ======================== */

/**
 * @brief  从对象池分配一个空闲槽位
 * @param  out_page  输出：指向分配的页面实例指针
 * @param  out_data  输出：指向分配的私有数据指针
 * @retval true   分配成功
 * @retval false  池已满
 */
static bool pool_alloc(sl_Page **out_page, sl_MenuPageData **out_data) {
    for (int i = 0; i < SL_MENU_PAGE_POOL_SIZE; i++) {
        if (!(pool_used & (1 << i))) {
            pool_used |= (uint8_t)(1 << i);
            *out_page = &menu_page_pool[i];
            *out_data = &menu_page_data_pool[i];
            return true;
        }
    }
    return false;
}

/**
 * @brief  释放对象池槽位
 * @param  page  指向需要释放的页面实例
 * @note   根据 page 在 pool 中的偏移计算索引，清除占用位
 */
static void pool_free(sl_Page *page) {
    int idx = (int)(page - menu_page_pool);
    if (idx >= 0 && idx < SL_MENU_PAGE_POOL_SIZE) {
        pool_used &= (uint8_t)~(1 << idx);
    }
}

/**
 * @brief  获取菜单项的显示文本
 * @param  item  指向菜单项（只读）
 * @retval 指向显示文本的只读指针
 * @note   根据菜单项类型，在基础文本后追加状态信息：
 *         - TOGGLE: 追加 " ON"/" OFF"
 *         - CHOICE: 追加当前选项文本
 *         - VALUE:  追加当前数值
 *         - SUB_MENU: 追加 " >"
 *         - ACTION: 仅显示基础文本
 *
 * @warning 使用静态缓冲区，非线程安全，每次调用覆盖上次结果
 */
static const char* get_item_display_text(const sl_MenuItem *item) {
    static char buf[32];
    if (!item || !item->text) {
        return "";
    }

    int len = (int)strlen(item->text);
    if (len > 24) {
        len = 24;
    }
    memcpy(buf, item->text, (size_t)len);
    buf[len] = '\0';

    switch (item->type) {
    case SL_MENU_TOGGLE: {
        const char *state = sl_menu_item_get_toggle_text(item);
        buf[len] = ' ';
        memcpy(buf + len + 1, state, strlen(state) + 1);
        break;
    }
    case SL_MENU_CHOICE: {
        const char *choice = sl_menu_item_get_choice_text(item);
        buf[len] = ' ';
        memcpy(buf + len + 1, choice, strlen(choice) + 1);
        break;
    }
    case SL_MENU_VALUE: {
        int16_t val = sl_menu_item_get_value(item);
        buf[len] = ' ';
        int vlen = snprintf(buf + len + 1, sizeof(buf) - len - 1, "%d", (int)val);
        (void)vlen;
        break;
    }
    case SL_MENU_SUB_MENU: {
        buf[len] = ' ';
        buf[len + 1] = '>';
        buf[len + 2] = '\0';
        break;
    }
    default:
        break;
    }

    return buf;
}

/**
 * @brief  构建列表项数组
 * @param  model  指向菜单页模型（只读）
 * @param  out    输出：列表项数组（调用方持有）
 * @param  count  菜单项数量
 * @note   将每个菜单项的显示文本映射为 sl_ListItem
 */
static void build_list_items(const sl_MenuPageModel *model,
                             sl_ListItem *out, int count) {
    for (int i = 0; i < count; i++) {
        out[i].text = get_item_display_text(&model->items[i]);
    }
}

/* ======================== 菜单页面生命周期回调 ======================== */

/**
 * @brief  菜单页面初始化回调
 * @param  self  指向当前页面实例
 *
 * 初始化流程：
 *   1. 从 self->data 获取私有数据；
 *   2. 优先使用 data->model，其次使用 self->arg（兼容两种传参方式）；
 *   3. 初始化列表视图控件，设置列表项数据；
 *   4. 初始化按键重复生成器；
 *   5. 订阅 UI 语义事件（presenter 回调）。
 */
static void menu_page_init_cb(sl_Page *self) {
    sl_MenuPageData *data = (sl_MenuPageData *)self->data;

    if (!data->model && self->arg) {
        data->model = (const sl_MenuPageModel *)self->arg;
    }

    const sl_MenuPageModel *model = data->model;
    if (!model) {
        return;
    }

    sl_list_view_init(&data->list_view, 0, 16, SL_DISP_WIDTH,
                      model->item_count > 2 ? 2 : model->item_count,
                      18, &sl_default_font);

    static sl_ListItem list_items[16];
    int count = model->item_count;
    if (count > 16) {
        count = 16;
    }
    build_list_items(model, list_items, count);
    sl_list_view_set_items(&data->list_view, list_items, count);

    sl_key_repeat_init(&data->key_repeat);
    data->editing = 0;

}

/**
 * @brief  菜单页面绘制回调
 * @param  self  指向当前页面实例
 *
 * 绘制流程：
 *   1. 绘制反色标题栏（显示模型标题）；
 *   2. 推进光标动画；
 *   3. 绘制列表视图控件树；
 *   4. 若处于值编辑模式，在标题栏显示编辑指示符。
 */
static void menu_page_draw_cb(sl_Page *self) {
    sl_MenuPageData *data = (sl_MenuPageData *)self->data;
    const sl_MenuPageModel *model = data->model;

    const char *title = (model && model->title) ? model->title : "";
    sl_disp_draw_title_bar(title, &sl_default_font);

    sl_widget_draw_tree(&data->list_view.base, 0, 0);

    if (data->editing) {
        sl_disp_draw_string(SL_DISP_WIDTH - 12, 1, "*", &sl_default_font, 0);
    }
}

/**
 * @brief  菜单页面事件处理回调
 * @param  self   指向当前页面实例
 * @param  event  指向原始输入事件
 * @retval 0      事件已处理，页面继续保持
 * @retval 1      请求退出当前页面
 *
 * 事件分发逻辑：
 *   1. 将事件传递给按键重复生成器（更新重复状态）；
 *   2. 将事件传递给列表视图控件（处理导航和确认）；
 *   3. 处理返回键：
 *      - 值编辑模式：退出编辑模式
 *      - 浏览模式：请求退出页面
 */
static int menu_page_proc_cb(sl_Page *self, const sl_Event *event) {
    sl_MenuPageData *data = (sl_MenuPageData *)self->data;

    sl_key_repeat_on_event(&data->key_repeat, event);

    if (data->editing) {
        if (event->type == SL_EVT_KEY_UP || event->type == SL_EVT_KEY_DOWN) {
            const sl_MenuPageModel *model = data->model;
            if (model && data->list_view.cursor < model->item_count) {
                sl_MenuItem *item = (sl_MenuItem *)&model->items[data->list_view.cursor];
                if (item->type == SL_MENU_VALUE) {
                    int step = (event->type == SL_EVT_KEY_UP) ? 1 : -1;
                    int16_t new_val = (int16_t)(sl_menu_item_get_value(item) + step);
                    if (new_val < item->min) {
                        new_val = item->min;
                    }
                    if (new_val > item->max) {
                        new_val = item->max;
                    }
                    sl_menu_item_set_value(item, new_val);

                    static sl_ListItem list_items[16];
                    int count = model->item_count;
                    if (count > 16) {
                        count = 16;
                    }
                    build_list_items(model, list_items, count);
                    sl_list_view_set_items(&data->list_view, list_items, count);
                    sl_page_request_redraw();
                    return 0;
                }
            }
        } else if (event->type == SL_EVT_KEY_ENTER || event->type == SL_EVT_KEY_BACK) {
            data->editing = 0;
            sl_page_request_redraw();
            return 0;
        }
        return 0;
    }

    sl_widget_dispatch_event(&data->list_view.base, event);

    if (event->type == SL_EVT_KEY_BACK) {
        return 1;
    }

    return 0;
}

/**
 * @brief  菜单页面退出回调
 * @param  self  指向当前页面实例
 * @note   取消订阅 UI 语义事件，释放对象池槽位
 */
static void menu_page_exit_cb(sl_Page *self) {
    sl_MenuPageData *data = (sl_MenuPageData *)self->data;
    pool_free(self);
    (void)data;
}

/**
 * @brief  菜单页面 Presenter 回调
 * @param  evt   指向 UI 语义事件
 * @param  page  指向产生事件的页面实例
 *
 * 语义事件处理逻辑：
 *   - FOCUS_CHANGED: 请求重绘（更新光标位置）
 *   - ENTER_ITEM: 根据菜单项类型执行对应操作：
 *     - SUB_MENU:  分配子菜单页面并推入栈
 *     - TOGGLE:    切换开关值，刷新列表
 *     - CHOICE:    循环切换选项，刷新列表
 *     - ACTION:    执行动作回调
 *     - VALUE:     进入值编辑模式
 */
static void menu_page_presenter_cb(const sl_UiEvent *evt, sl_Page *page) {
    sl_MenuPageData *data = (sl_MenuPageData *)page->data;
    const sl_MenuPageModel *model = data->model;
    if (!model) {
        return;
    }

    if (evt->type == SL_UI_EVT_FOCUS_CHANGED) {
        sl_page_request_redraw();
        return;
    }

    if (evt->type == SL_UI_EVT_ENTER_ITEM) {
        int idx = evt->value;
        if (idx < 0 || idx >= model->item_count) {
            return;
        }

        sl_MenuItem *item = (sl_MenuItem *)&model->items[idx];

        switch (item->type) {
        case SL_MENU_SUB_MENU:
            if (item->sub) {
                sl_Page *sub_page = sl_menu_page_alloc(item->sub);
                if (sub_page) {
                    sl_page_enter(sub_page);
                }
            }
            break;

        case SL_MENU_TOGGLE:
            sl_menu_item_toggle(item);
            {
                static sl_ListItem list_items[16];
                int count = model->item_count;
                if (count > 16) {
                    count = 16;
                }
                build_list_items(model, list_items, count);
                sl_list_view_set_items(&data->list_view, list_items, count);
                sl_page_request_redraw();
            }
            break;

        case SL_MENU_CHOICE:
            sl_menu_item_next_choice(item);
            {
                static sl_ListItem list_items2[16];
                int count2 = model->item_count;
                if (count2 > 16) {
                    count2 = 16;
                }
                build_list_items(model, list_items2, count2);
                sl_list_view_set_items(&data->list_view, list_items2, count2);
                sl_page_request_redraw();
            }
            break;

        case SL_MENU_ACTION:
            if (item->on_action) {
                item->on_action(item);
            }
            break;

        case SL_MENU_VALUE:
            data->editing = 1;
            sl_page_request_redraw();
            break;

        default:
            break;
        }
    }
}

/* ======================== 菜单页面公共接口 ======================== */

/**
 * @brief  从对象池分配一个菜单页面实例
 * @param  model  指向菜单页模型（外部持有，页面生命周期内有效）
 * @retval 指向已初始化的 sl_Page 指针，池满时返回 NULL
 * @note   分配后页面已初始化（init/draw/proc/exit/presenter 回调已设置），
 *         data->model 直接设置为传入的 model 指针
 */
sl_Page* sl_menu_page_alloc(const sl_MenuPageModel *model) {
    sl_Page *page = NULL;
    sl_MenuPageData *data = NULL;

    if (!pool_alloc(&page, &data)) {
        return NULL;
    }

    memset(page, 0, sizeof(*page));
    memset(data, 0, sizeof(*data));

    page->name      = "menu_page";
    page->init      = menu_page_init_cb;
    page->draw      = menu_page_draw_cb;
    page->proc      = menu_page_proc_cb;
    page->exit      = menu_page_exit_cb;
    page->presenter = menu_page_presenter_cb;
    page->data      = data;
    page->arg       = NULL;

    data->model = model;

    return page;
}
