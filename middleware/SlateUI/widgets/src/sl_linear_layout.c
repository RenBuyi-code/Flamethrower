/**
 * @file    sl_linear_layout.c
 * @brief   SlateUI 线性布局控件实现
 *
 * 本文件实现水平/垂直线性布局的坐标计算：
 *   · 容器本身不绘制任何背景，仅负责子控件的坐标管理。
 *   · 调用 sl_linear_layout_apply() 后，子控件坐标被重新计算：
 *     - 水平布局：子控件从左到右排列，x 依次递增
 *     - 垂直布局：子控件从上到下排列，y 依次递增
 *   · 不可见的子控件跳过，不占空间。
 *   · 子控件的尺寸（w/h）保持不变，仅修改 x/y 坐标。
 *
 * 注意：
 *   若需背景，可将布局嵌套在其他有背景绘制的控件内。
 */

#include "../inc/sl_linear_layout.h"

/* ======================== 内部绘制回调 ======================== */

/**
 * @brief  线性布局绘制回调（空实现）
 * @param  self       指向控件基类
 * @param  offset_x   父级累积 X 偏移
 * @param  offset_y   父级累积 Y 偏移
 * @note   容器本身不产生视觉内容，
 *         子控件的绘制由基类的 draw_tree 递归完成
 */
static void layout_draw(sl_Widget *self, int offset_x, int offset_y) {
    (void)self;
    (void)offset_x;
    (void)offset_y;
}

/* ======================== 线性布局接口实现 ======================== */

/**
 * @brief  初始化线性布局控件
 * @param  layout   布局对象指针
 * @param  x        容器左上角 X 坐标
 * @param  y        容器左上角 Y 坐标
 * @param  w        容器宽度（像素）
 * @param  h        容器高度（像素）
 * @param  orient   排列方向 (SL_LAYOUT_HORIZONTAL / SL_LAYOUT_VERTICAL)
 * @param  spacing  子控件间距（像素）
 */
void sl_linear_layout_init(sl_LinearLayout *layout, int x, int y, int w, int h,
                           sl_LayoutOrientation orient, uint8_t spacing) {
    sl_widget_init(&layout->base, x, y, w, h, layout_draw, NULL);
    layout->orient  = orient;
    layout->spacing = spacing;
}

/**
 * @brief  重新计算并应用子控件位置
 * @param  layout  指向布局实例
 *
 * 遍历所有可见子控件，按添加顺序依次排列：
 *   - 水平布局：child->x = pos, child->y = 0, pos += child->w + spacing
 *   - 垂直布局：child->x = 0, child->y = pos, pos += child->h + spacing
 * 不可见的子控件跳过，不占用布局空间。
 *
 * @note   应在添加/移除子控件或改变可见性后调用
 */
void sl_linear_layout_apply(sl_LinearLayout *layout) {
    sl_Widget *child = layout->base.first_child;
    int pos = 0;

    while (child) {
        if (child->flags & SL_WIDGET_FLAG_VISIBLE) {
            if (layout->orient == SL_LAYOUT_HORIZONTAL) {
                child->x = (int16_t)pos;
                child->y = 0;
                pos += child->w + layout->spacing;
            } else {
                child->x = 0;
                child->y = (int16_t)pos;
                pos += child->h + layout->spacing;
            }
        }
        child = child->next_sibling;
    }
}
