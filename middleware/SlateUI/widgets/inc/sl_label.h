/**
 * @file    sl_label.h
 * @brief   SlateUI 文本标签控件
 *
 * 本模块实现文本标签控件，支持对齐方式和自动滚动：
 *   · 支持左对齐、居中、右对齐三种水平对齐方式。
 *   · 当文本宽度超过控件宽度时，支持自动水平滚动（跑马灯效果）。
 *   · 滚动由 sl_label_tick() 驱动，需在主循环中周期性调用。
 *   · 滚动速度和暂停帧数可通过编译选项配置。
 *
 * 典型用法：
 *   sl_Label title;
 *   sl_label_init(&title, 0, 0, 128, 10, "Hello", font, 1, SL_LABEL_ALIGN_LEFT);
 *   sl_label_set_scroll(&title, SL_LABEL_SCROLL_AUTO);
 *   // 主循环中：
 *   sl_label_tick(&title);
 */

#ifndef SL_LABEL_H
#define SL_LABEL_H

#include "sl_widget.h"

/* ======================== 标签控件结构体 ======================== */

/**
 * @brief  文本标签控件结构体
 *
 * 继承 sl_Widget 基类，增加文本内容、字体、颜色、
 * 对齐方式和滚动状态等属性。
 */
typedef struct {
    sl_Widget    base;           /**< 控件基类（必须为第一个成员） */
    const char  *text;           /**< 文本内容指针（外部持有，不拷贝） */
    const void  *font;           /**< 字体指针（NULL 使用默认字体） */
    uint8_t      color;          /**< 文字颜色 (0=灭, 非0=亮) */
    uint8_t      align;          /**< 水平对齐方式 (SL_LABEL_ALIGN_xxx) */
    uint8_t      scroll;         /**< 滚动模式 (SL_LABEL_SCROLL_xxx) */
    int16_t      scroll_offset;  /**< 当前滚动偏移量（像素，正值向左） */
    int16_t      scroll_pause;   /**< 滚动暂停倒计时（帧数） */
} sl_Label;

/* ======================== 对齐方式定义 ======================== */

/** @brief 左对齐 */
#define SL_LABEL_ALIGN_LEFT   0

/** @brief 居中对齐 */
#define SL_LABEL_ALIGN_CENTER 1

/** @brief 右对齐 */
#define SL_LABEL_ALIGN_RIGHT  2

/* ======================== 滚动模式定义 ======================== */

/** @brief 不滚动（文本超出时截断） */
#define SL_LABEL_SCROLL_NONE  0

/** @brief 自动滚动（文本超出控件宽度时启动跑马灯） */
#define SL_LABEL_SCROLL_AUTO  1

/**
 * @brief  滚动速度（像素/帧）
 * @note   可在编译选项中覆盖
 */
#ifndef SL_LABEL_SCROLL_SPEED
#define SL_LABEL_SCROLL_SPEED 2
#endif

/**
 * @brief  滚动到两端时的暂停帧数
 * @note   可在编译选项中覆盖
 */
#ifndef SL_LABEL_SCROLL_PAUSE_FRAMES
#define SL_LABEL_SCROLL_PAUSE_FRAMES 30
#endif

/* ======================== 标签控件接口 ======================== */

/**
 * @brief  初始化文本标签控件
 * @param  label  指向标签控件实例
 * @param  x      控件左上角 X 坐标
 * @param  y      控件左上角 Y 坐标
 * @param  w      控件宽度（像素）
 * @param  h      控件高度（像素）
 * @param  text   文本内容指针（外部持有，不拷贝）
 * @param  font   字体指针（NULL 使用默认字体）
 * @param  color  文字颜色 (0=灭, 非0=亮)
 * @param  align  水平对齐方式 (SL_LABEL_ALIGN_xxx)
 * @note   默认不滚动，需调用 sl_label_set_scroll() 启用
 */
void sl_label_init(sl_Label *label, int x, int y, int w, int h,
                   const char *text, const void *font,
                   uint8_t color, uint8_t align);

/**
 * @brief  设置标签滚动模式
 * @param  label  指向标签控件实例
 * @param  mode   滚动模式 (SL_LABEL_SCROLL_NONE / SL_LABEL_SCROLL_AUTO)
 * @note   启用自动滚动后，需在主循环中调用 sl_label_tick()
 */
void sl_label_set_scroll(sl_Label *label, uint8_t mode);

/**
 * @brief  标签控件时钟节拍
 * @param  label  指向标签控件实例
 * @note   驱动自动滚动动画，需在主循环中周期性调用；
 *         滚动到两端时暂停 SL_LABEL_SCROLL_PAUSE_FRAMES 帧
 */
void sl_label_tick(sl_Label *label);

#endif
