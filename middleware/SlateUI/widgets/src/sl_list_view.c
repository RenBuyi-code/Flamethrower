/**
 * @file    sl_list_view.c
 * @brief   SlateUI 列表视图控件实现
 *
 * 本文件实现垂直滚动列表的绘制、事件处理和光标动画：
 *   · 绘制：逐项绘制文本，光标所在项反色显示（亮底暗字）；
 *     光标位置由 cursor_tween 插值动画驱动，实现平滑过渡。
 *   · 事件：上下键移动光标（支持循环），确认键投递 ENTER_ITEM 语义事件；
 *     光标移动时自动调整 scroll_offset 确保当前项可见。
 *   · 滚动条：可选 2px 宽的滚动条指示器，显示视口在列表中的位置。
 */

#include "../inc/sl_list_view.h"
#include "../../core/inc/sl_display.h"
#include "../../core/inc/sl_page_manager.h"
#include "../../font/sl_font.h"
#include <string.h>

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  列表视图绘制回调
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 *
 * 绘制流程：
 *   1. 计算光标 Y 坐标（动画中取插值，否则直接计算）；
 *   2. 逐项绘制：与光标重叠的项反色显示，其余正常显示；
 *   3. 若启用滚动条，在右侧绘制滚动条滑块。
 */
static void list_draw(sl_Widget *self, int offset_x, int offset_y) {
    sl_ListView *lv = (sl_ListView *)self;
    const sl_Font *font = (const sl_Font *)(lv->font ? lv->font : &sl_default_font);

    int item_h = lv->item_height;
    int visible = lv->visible_count;
    int content_w = lv->show_scrollbar ? (self->w - 3) : self->w;

    int cursor_y;
    if (sl_tween_is_active(&lv->cursor_tween)) {
        cursor_y = (int)sl_tween_get_value(&lv->cursor_tween);
    } else {
        cursor_y = (lv->cursor - lv->scroll_offset) * item_h;
    }

    for (int i = 0; i < visible; i++) {
        int data_idx = i + lv->scroll_offset;
        int y = offset_y + i * item_h;
        if (data_idx >= lv->item_count) {
            sl_disp_fill_rect(offset_x, y, content_w, item_h, 0);
            continue;
        }

        const char *text = lv->items[data_idx].text;
        if (!text) text = "";

        int item_top = i * item_h;
        int item_bot = item_top + item_h;
        int cursor_top = cursor_y;
        int cursor_bot = cursor_y + item_h;

        if (item_top < cursor_bot && item_bot > cursor_top) {
            sl_disp_fill_rect(offset_x, y, content_w, item_h, 1);
            sl_disp_draw_string(offset_x + 4, y, text, font, 0);
        } else {
            sl_disp_fill_rect(offset_x, y, content_w, item_h, 0);
            sl_disp_draw_string(offset_x + 4, y, text, font, 1);
        }
    }

    if (lv->show_scrollbar && lv->item_count > visible) {
        int bar_x = offset_x + self->w - 2;
        int total_h = visible * item_h;
        int thumb_h = (visible * total_h) / lv->item_count;
        if (thumb_h < 3) thumb_h = 3;
        int max_scroll = lv->item_count - visible;
        int track_h = total_h - thumb_h;
        int thumb_y = offset_y;
        if (max_scroll > 0) {
            thumb_y = offset_y + (lv->scroll_offset * track_h) / max_scroll;
        }
        sl_disp_fill_rect(bar_x, offset_y, 2, total_h, 0);
        sl_disp_fill_rect(bar_x, thumb_y, 2, thumb_h, 1);
    }
}

/* ======================== 内部辅助函数 ======================== */

/**
 * @brief  投递焦点变化语义事件
 * @param  self         指向控件基类
 * @param  new_cursor   新光标索引
 * @param  prev_cursor  旧光标索引
 * @note   若新旧索引相同则不投递
 */
static void post_focus_changed(sl_Widget *self, int new_cursor, int prev_cursor) {
    if (new_cursor == prev_cursor) return;
    sl_UiEvent ui_evt = {
        .type       = SL_UI_EVT_FOCUS_CHANGED,
        .widget_id  = self->id,
        .action_id  = SL_ACTION_NONE,
        .value      = new_cursor,
        .value_prev = prev_cursor,
        .context    = NULL
    };
    sl_ui_event_post(&ui_evt);
}

/**
 * @brief  启动光标移动动画
 * @param  lv           指向列表视图实例
 * @param  prev_cursor  移动前的光标索引
 * @note   从旧位置到新位置启动缓出插值动画
 */
static void start_cursor_anim(sl_ListView *lv, int prev_cursor) {
    int from_y = (prev_cursor - lv->scroll_offset) * lv->item_height;
    int to_y   = (lv->cursor - lv->scroll_offset) * lv->item_height;
    sl_tween_start(&lv->cursor_tween, from_y, to_y,
                   SL_LIST_CURSOR_ANIM_MS, SL_TWEEN_EASE_OUT);
}

/* ======================== 内部事件处理回调 ======================== */

/**
 * @brief  列表视图事件处理回调
 * @param  self   指向控件基类
 * @param  event  指向原始输入事件
 * @retval true   事件已消费
 * @retval false  事件未消费
 *
 * 事件处理逻辑：
 *   - KEY_UP:   光标上移，到达顶部后循环到底部；自动调整 scroll_offset
 *   - KEY_DOWN: 光标下移，到达底部后循环到顶部；自动调整 scroll_offset
 *   - KEY_ENTER: 投递 SL_UI_EVT_ENTER_ITEM 语义事件，不消费原始事件
 */
static bool list_proc(sl_Widget *self, const sl_Event *event) {
    sl_ListView *lv = (sl_ListView *)self;

    if (!(self->flags & SL_WIDGET_FLAG_VISIBLE) ||
        !(self->flags & SL_WIDGET_FLAG_FOCUSABLE)) {
        return false;
    }

    switch (event->type) {
    case SL_EVT_KEY_UP:
        if (lv->item_count == 0) break;

        if (lv->cursor > 0) {
            int prev = lv->cursor;
            lv->cursor--;
            if (lv->cursor < lv->scroll_offset) {
                lv->scroll_offset = lv->cursor;
            }
            start_cursor_anim(lv, prev);
            post_focus_changed(self, lv->cursor, prev);
        } else {
            int prev = lv->cursor;
            lv->cursor = lv->item_count - 1;
            lv->scroll_offset = (lv->item_count > lv->visible_count) ?
                                (lv->item_count - lv->visible_count) : 0;
            start_cursor_anim(lv, prev);
            post_focus_changed(self, lv->cursor, prev);
        }
        return true;

    case SL_EVT_KEY_DOWN:
        if (lv->item_count == 0) break;

        if (lv->cursor < lv->item_count - 1) {
            int prev = lv->cursor;
            lv->cursor++;
            if (lv->cursor >= lv->scroll_offset + lv->visible_count) {
                lv->scroll_offset = lv->cursor - lv->visible_count + 1;
            }
            start_cursor_anim(lv, prev);
            post_focus_changed(self, lv->cursor, prev);
        } else {
            int prev = lv->cursor;
            lv->cursor = 0;
            lv->scroll_offset = 0;
            start_cursor_anim(lv, prev);
            post_focus_changed(self, lv->cursor, prev);
        }
        return true;

    case SL_EVT_KEY_ENTER: {
        sl_UiEvent ui_evt = {
            .type       = SL_UI_EVT_ENTER_ITEM,
            .widget_id  = self->id,
            .action_id  = SL_ACTION_NONE,
            .value      = lv->cursor,
            .value_prev = 0,
            .context    = NULL
        };
        sl_ui_event_post(&ui_evt);
        return false;
    }

    default:
        break;
    }
    return false;
}

/* ======================== 列表视图接口实现 ======================== */

/**
 * @brief  初始化列表视图控件
 * @param  lv             指向列表视图实例
 * @param  x              控件左上角 X 坐标
 * @param  y              控件左上角 Y 坐标
 * @param  w              控件宽度（像素）
 * @param  visible_count  可见项数量
 * @param  item_height    每项高度（像素）
 * @param  font           字体指针
 * @note   控件高度 = visible_count * item_height
 */
void sl_list_view_init(sl_ListView *lv, int x, int y, int w,
                       int visible_count, int item_height,
                       const void *font) {
    int h = visible_count * item_height;
    sl_widget_init(&lv->base, x, y, w, h, list_draw, list_proc);

    lv->font          = font;
    lv->visible_count = visible_count;
    lv->item_height   = item_height;
    lv->items         = NULL;
    lv->item_count    = 0;
    lv->scroll_offset = 0;
    lv->cursor        = 0;
    lv->show_scrollbar = 0;
    memset(&lv->cursor_tween, 0, sizeof(lv->cursor_tween));
}

/**
 * @brief  设置列表项数据
 * @param  lv     指向列表视图实例
 * @param  items  列表项数组指针（外部持有）
 * @param  count  列表项数量
 * @note   调用后光标和滚动偏移重置，动画状态清零
 */
void sl_list_view_set_items(sl_ListView *lv, const sl_ListItem *items, int count) {
    lv->items      = items;
    lv->item_count = count;
    lv->cursor     = 0;
    lv->scroll_offset = 0;
    memset(&lv->cursor_tween, 0, sizeof(lv->cursor_tween));
}

/**
 * @brief  设置是否显示滚动条
 * @param  lv    指向列表视图实例
 * @param  show  1=显示，0=隐藏
 */
void sl_list_view_set_scrollbar(sl_ListView *lv, uint8_t show) {
    if (lv) lv->show_scrollbar = show;
}

/**
 * @brief  列表视图时钟节拍
 * @param  lv        指向列表视图实例
 * @param  delta_ms  距上次调用经过的毫秒数
 * @note   推进光标动画插值器，动画期间持续请求重绘
 */
void sl_list_view_tick(sl_ListView *lv, uint16_t delta_ms) {
    if (!lv) return;
    if (sl_tween_is_active(&lv->cursor_tween)) {
        sl_tween_tick(&lv->cursor_tween, delta_ms);
        sl_page_request_redraw();
    }
}
