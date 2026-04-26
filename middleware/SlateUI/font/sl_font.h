/**
 * @file    sl_font.h
 * @brief   SlateUI 字体绘制抽象接口
 *
 * 本模块定义字体绘制的抽象接口，允许用户以插件形式挂接任意字体：
 *   · sl_Font 结构体包含字库数据、字符尺寸和绘制回调函数。
 *   · 框架通过 sl_font_draw_internal() 代理函数调用字体绘制，
 *     不感知具体字库细节。
 *   · 全局默认字体 sl_default_font 为 8x16 ASCII（sl_font_ascii_16x16.c）。
 *   · 中文字体 sl_font_chinese 为 16x16 GB2312 常用字子集。
 *   · 用户可注册自定义字体，只需实现 sl_FontDraw 回调。
 *
 * 当前可用字体：
 *   ┌─────────────────────┬────────┬────────┐
 *   │ 字体名称              │ 宽度    │ 高度    │
 *   ├─────────────────────┼────────┼────────┤
 *   │ sl_default_font      │ 8 px   │ 16 px  │ (ASCII 默认)
 *   │ sl_font_chinese      │ 16 px  │ 16 px  │ (中文 GB2312 子集)
 *   └─────────────────────┴────────┴────────┘
 *
 * LCD12232 (122×32) 屏幕布局参考：
 *   ┌──────────────────────────────────────┐
 *   │ 标题栏 (16px, 8x16 ASCII)           │ y=0
 *   ├──────────────────────────────────────┤ y=16
 *   │ 菜单项/内容区 (16px)                │ 仅 1 行 8x16 或 1 行 16x16 中文
 *   ├──────────────────────────────────────┤ y=32
 *   └──────────────────────────────────────┘
 */

#ifndef SL_FONT_H
#define SL_FONT_H

#include <stdint.h>

/* ======================== 字体绘制回调类型 ======================== */

/**
 * @brief  字体绘制回调函数原型
 * @param  x      起始像素 X 坐标（左上角）
 * @param  y      起始像素 Y 坐标（左上角）
 * @param  str    待绘制的字符串
 * @param  font   字体对象指针（NULL 使用默认字体）
 * @param  color  绘制颜色 (0=灭, 非0=亮)
 * @retval 下一个可写字符的 X 坐标
 */
typedef uint16_t (*sl_FontDraw)(uint16_t x, uint16_t y, const char *str,
                                const void *font, uint8_t color);

/* ======================== 字体结构体 ======================== */

/**
 * @brief  字体结构体
 *
 * 描述一个字体的属性和绘制回调：
 *   - data:   字库数据指针（格式由具体字体实现定义，可为 NULL）
 *   - width:  单字符宽度（像素），用于布局计算
 *   - height: 单字符高度（像素），用于布局计算
 *   - draw:   绘制回调函数，负责将字符串渲染到显存
 */
typedef struct {
    const void *data;       /**< 字库数据指针（可为 NULL） */
    uint8_t     width;      /**< 单字符宽度（像素） */
    uint8_t     height;     /**< 单字符高度（像素） */
    sl_FontDraw draw;       /**< 绘制回调函数 */
} sl_Font;

/* ======================== 全局字体对象 ======================== */

/** @brief 全系统默认字体（8x16 ASCII，在 sl_font_ascii_16x16.c 中定义） */
extern sl_Font sl_default_font;

/** @brief 中文字体（16x16 GB2312 常用字子集，在 sl_font_chinese_16x16.c 中定义） */
extern sl_Font sl_font_chinese;

/* ======================== 内部代理函数 ======================== */

/**
 * @brief  字体绘制内部代理函数
 * @param  x      起始像素 X 坐标
 * @param  y      起始像素 Y 坐标
 * @param  str    待绘制的字符串
 * @param  font   字体对象指针（NULL 使用默认字体）
 * @param  color  绘制颜色 (0=灭, 非0=亮)
 * @retval 下一个可写字符的 X 坐标；无绘制函数时返回原 x
 * @note   由 sl_disp_draw_string() 内部调用，不应由用户直接调用
 */
uint16_t sl_font_draw_internal(uint16_t x, uint16_t y, const char *str,
                               const void *font, uint8_t color);

#endif
