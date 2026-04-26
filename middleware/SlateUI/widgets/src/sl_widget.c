/**
 * @file    sl_widget.c
 * @brief   SlateUI 控件基类实现 —— 控件树构建、递归绘制与事件分发
 *
 * 本文件实现控件树的核心操作：
 *   · 控件初始化：设置坐标、尺寸、回调函数和标志位
 *   · 控件树操作：添加/移除子控件（first-child / next-sibling 结构）
 *   · 递归绘制：sl_widget_draw_tree() 先绘制自身，再递归绘制子控件
 *   · 事件分发：sl_widget_dispatch_event() 子控件优先（后添加的在上层），
 *     第一个返回 true 的控件消费事件
 *   · ID 查找：sl_widget_find_by_id() 递归搜索控件树
 *
 * 设计约束：
 *   - 本文件不含任何硬件相关代码，所有绘制操作通过 sl_disp_* 函数完成
 *   - 控件树为静态分配，无动态内存操作
 *   - 事件分发为深度优先、后序遍历（子控件优先于父控件）
 */

#include "../inc/sl_widget.h"
#include <stddef.h>

/* ======================== 控件初始化 ======================== */

/**
 * @brief  初始化控件基类
 * @param  w      控件指针（由用户静态分配）
 * @param  x      相对父容器的 X 坐标
 * @param  y      相对父容器的 Y 坐标
 * @param  w_px   控件宽度（像素）
 * @param  h_px   控件高度（像素）
 * @param  draw   绘制回调（可为 NULL，此时仅递归绘制子控件）
 * @param  proc   事件处理回调（可为 NULL，表示不处理事件）
 * @note   初始化后 flags = VISIBLE | FOCUSABLE，无父/子/兄弟
 */
void sl_widget_init(sl_Widget *w, int x, int y, int w_px, int h_px,
                    sl_WidgetDraw draw, sl_WidgetProc proc) {
    w->id = SL_WIDGET_ID_NONE;
    w->x = (int16_t)x;
    w->y = (int16_t)y;
    w->w = (int16_t)w_px;
    w->h = (int16_t)h_px;

    w->parent       = NULL;
    w->first_child  = NULL;
    w->next_sibling = NULL;

    w->draw = draw;
    w->proc = proc;

    w->flags = SL_WIDGET_FLAG_VISIBLE | SL_WIDGET_FLAG_FOCUSABLE;
    w->user_data = NULL;
}

/* ======================== 控件树操作 ======================== */

/**
 * @brief  向父控件添加子控件（插入子链表尾部）
 * @param  parent  父控件指针
 * @param  child   要添加的子控件指针
 * @note   子控件的 parent 指针自动设置为 parent；
 *         若父控件尚无子控件，则直接设为 first_child
 */
void sl_widget_add_child(sl_Widget *parent, sl_Widget *child) {
    if (!parent || !child) return;

    child->parent = parent;

    if (!parent->first_child) {
        parent->first_child = child;
        return;
    }

    sl_Widget *sib = parent->first_child;
    while (sib->next_sibling) {
        sib = sib->next_sibling;
    }
    sib->next_sibling = child;
}

/**
 * @brief  从父控件中移除一个子控件（不释放内存）
 * @param  parent  父控件指针
 * @param  child   要移除的子控件指针
 * @note   使用间接指针技巧（二级指针）简化链表操作，
 *         移除后 child->parent 和 child->next_sibling 置 NULL
 */
void sl_widget_remove_child(sl_Widget *parent, sl_Widget *child) {
    if (!parent || !child) return;

    sl_Widget **indirect = &parent->first_child;
    while (*indirect) {
        if (*indirect == child) {
            *indirect = child->next_sibling;
            child->parent = NULL;
            child->next_sibling = NULL;
            return;
        }
        indirect = &(*indirect)->next_sibling;
    }
}

/* ======================== 递归绘制 ======================== */

/**
 * @brief  递归绘制控件树
 * @param  root      控件树的根
 * @param  offset_x  当前累积的 X 偏移（根调用时通常为 0）
 * @param  offset_y  当前累积的 Y 偏移
 * @note   仅绘制 flags 包含 VISIBLE 的控件；
 *         绝对坐标 = offset + root->x/y；
 *         先绘制自身（如有 draw 回调），再递归绘制子控件
 */
void sl_widget_draw_tree(sl_Widget *root, int offset_x, int offset_y) {
    if (!root) return;

    if (!(root->flags & SL_WIDGET_FLAG_VISIBLE)) {
        return;
    }

    int abs_x = offset_x + root->x;
    int abs_y = offset_y + root->y;

    if (root->draw) {
        root->draw(root, abs_x, abs_y);
    }

    sl_Widget *child = root->first_child;
    while (child) {
        sl_widget_draw_tree(child, abs_x, abs_y);
        child = child->next_sibling;
    }
}

/* ======================== 事件分发 ======================== */

/**
 * @brief  反向递归遍历子控件链表（后添加的优先处理）
 * @param  child  当前子控件指针
 * @param  event  待分发的事件
 * @retval true   事件已被某个子控件消费
 * @retval false  事件未被消费
 * @note   使用递归实现反向遍历：先递归到链表末尾，
 *         再从末尾向前处理，确保后添加的控件（视觉上层）优先
 */
static bool dispatch_children_reverse(sl_Widget *child, const sl_Event *event) {
    if (!child) {
        return false;
    }

    if (dispatch_children_reverse(child->next_sibling, event)) {
        return true;
    }

    return sl_widget_dispatch_event(child, event);
}

/**
 * @brief  将事件分发给控件树处理
 * @param  root   控件树的根
 * @param  event  待分发的事件
 * @retval true   事件已被某个控件消费
 * @retval false  事件未被任何控件处理
 *
 * 分发策略（子控件优先，后添加的在上层）：
 *   1. 反向遍历子链表，后添加的子控件优先处理；
 *   2. 若某个子控件消费了事件（返回 true），停止传播；
 *   3. 若所有子控件均未消费，尝试让当前控件自身处理（调用 proc）。
 */
bool sl_widget_dispatch_event(sl_Widget *root, const sl_Event *event) {
    if (!root || !(root->flags & SL_WIDGET_FLAG_VISIBLE)) {
        return false;
    }

    if (dispatch_children_reverse(root->first_child, event)) {
        return true;
    }

    if (root->proc && root->proc(root, event)) {
        return true;
    }

    return false;
}

/* ======================== Widget ID 操作 ======================== */

/**
 * @brief  设置控件 ID
 * @param  widget  指向控件实例
 * @param  id      控件标识符
 */
void sl_widget_set_id(sl_Widget *widget, sl_widget_id_t id) {
    if (widget) {
        widget->id = id;
    }
}

/**
 * @brief  获取控件 ID
 * @param  widget  指向控件实例（只读）
 * @retval 控件标识符，widget 为 NULL 时返回 SL_WIDGET_ID_NONE
 */
sl_widget_id_t sl_widget_get_id(const sl_Widget *widget) {
    return widget ? widget->id : SL_WIDGET_ID_NONE;
}

/**
 * @brief  递归查找指定 ID 的控件（可修改版本）
 * @param  root  根控件
 * @param  id    目标 ID
 * @retval 指向找到的控件指针，未找到返回 NULL
 */
static sl_Widget *find_by_id_impl(sl_Widget *root, sl_widget_id_t id) {
    if (!root) return NULL;
    if (root->id == id) return root;

    sl_Widget *child = root->first_child;
    while (child) {
        sl_Widget *found = find_by_id_impl(child, id);
        if (found) return found;
        child = child->next_sibling;
    }
    return NULL;
}

/**
 * @brief  根据控件 ID 在控件树中查找控件
 * @param  root  指向根控件
 * @param  id    目标控件 ID
 * @retval 指向找到的控件指针，未找到返回 NULL
 */
sl_Widget *sl_widget_find_by_id(sl_Widget *root, sl_widget_id_t id) {
    return find_by_id_impl(root, id);
}

/**
 * @brief  递归查找指定 ID 的控件（只读版本）
 * @param  root  根控件（只读）
 * @param  id    目标 ID
 * @retval 指向找到的控件只读指针，未找到返回 NULL
 */
static const sl_Widget *find_by_id_const_impl(const sl_Widget *root, sl_widget_id_t id) {
    if (!root) return NULL;
    if (root->id == id) return root;

    const sl_Widget *child = root->first_child;
    while (child) {
        const sl_Widget *found = find_by_id_const_impl(child, id);
        if (found) return found;
        child = child->next_sibling;
    }
    return NULL;
}

/**
 * @brief  根据控件 ID 在控件树中查找控件（只读）
 * @param  root  指向根控件（只读）
 * @param  id    目标控件 ID
 * @retval 指向找到的控件只读指针，未找到返回 NULL
 */
const sl_Widget *sl_widget_find_by_id_const(const sl_Widget *root, sl_widget_id_t id) {
    return find_by_id_const_impl(root, id);
}
