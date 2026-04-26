/**
 * @file    sl_display.c
 * @brief   SlateUI 显存缓冲区与绘制接口实现
 *
 * 本文件实现显存管理、像素绘制和物理屏幕刷新：
 *   · 显存组织（兼容 SSD1306 页寻址）：
 *     byte_idx = (y / 8) * SL_DISP_WIDTH + x
 *     每字节的 bit(y % 8) 代表该像素的亮灭。
 *   · 脏矩形追踪：每次写像素自动扩展脏矩形边界，
 *     flush 时仅发送脏区覆盖的页数据，减少 SPI 传输量。
 *   · 全局绘制偏移 (g_offset_x/y)：用于页面过渡动画中的滑动效果。
 *   · 可选双缓冲 (ping-pong)：DMA 异步发送时避免读写冲突。
 */

#include "../inc/sl_display.h"
#include "../../port/sl_port.h"
#include <string.h>

/* ======================== 全局显存缓冲区 ======================== */

/** @brief 主绘制缓冲区（静态分配，SL_DISP_BUF_SIZE 字节） */
static uint8_t draw_buf[SL_DISP_BUF_SIZE];

#if SL_USE_PINGPONG_BUF
/** @brief 刷新用缓冲区（双缓冲模式下，DMA 发送此缓冲区数据） */
static uint8_t flush_buf[SL_DISP_BUF_SIZE];
#endif

/* ======================== 脏矩形状态 ======================== */

/**
 * @brief  脏矩形数据结构
 *
 * 记录自上次 flush 以来被修改的矩形区域边界。
 * is_empty=1 表示无脏区，无需刷新。
 */
static struct {
    int x1;         /**< 脏区左边界（含） */
    int y1;         /**< 脏区上边界（含） */
    int x2;         /**< 脏区右边界（含） */
    int y2;         /**< 脏区下边界（含） */
    int is_empty;   /**< 脏区是否为空（1=空，0=有脏区） */
} dirty = {0, 0, 0, 0, 1};

/** @brief 全局绘制 X 偏移量（像素），由 sl_disp_set_offset 设置 */
static int g_offset_x = 0;

/** @brief 全局绘制 Y 偏移量（像素），由 sl_disp_set_offset 设置 */
static int g_offset_y = 0;

/* ======================== 内部辅助函数 ======================== */

/**
 * @brief  将单个像素坐标加入脏矩形
 * @param  x  像素 X 坐标（已偏移后，屏幕坐标系）
 * @param  y  像素 Y 坐标（已偏移后，屏幕坐标系）
 * @note   若坐标超出屏幕范围则忽略；
 *         首次调用时初始化脏矩形为该点，后续调用扩展边界
 */
static void dirty_add_pixel(int x, int y) {
    if (x < 0 || x >= SL_DISP_WIDTH || y < 0 || y >= SL_DISP_HEIGHT) return;

    if (dirty.is_empty) {
        dirty.x1 = dirty.x2 = x;
        dirty.y1 = dirty.y2 = y;
        dirty.is_empty = 0;
    } else {
        if (x < dirty.x1) dirty.x1 = x;
        if (x > dirty.x2) dirty.x2 = x;
        if (y < dirty.y1) dirty.y1 = y;
        if (y > dirty.y2) dirty.y2 = y;
    }
}

/* ======================== 核心绘制接口实现 ======================== */

/**
 * @brief  初始化显存缓冲区与脏矩形
 * @note   清空整块缓冲区并标记全屏脏区，下次 flush 将发送全部像素
 */
void sl_disp_init(void) {
    memset(draw_buf, 0, sizeof(draw_buf));
#if SL_USE_PINGPONG_BUF
    memset(flush_buf, 0, sizeof(flush_buf));
#endif
    dirty.is_empty = 0;
    dirty.x1 = 0;
    dirty.y1 = 0;
    dirty.x2 = SL_DISP_WIDTH - 1;
    dirty.y2 = SL_DISP_HEIGHT - 1;
}

/**
 * @brief  获取显存缓冲区首地址
 * @retval 指向 draw_buf 的指针
 */
uint8_t* sl_disp_get_buffer(void) {
    return draw_buf;
}

/**
 * @brief  绘制单个像素
 * @param  x      像素 X 坐标（0 ~ SL_DISP_WIDTH-1，偏移前）
 * @param  y      像素 Y 坐标（0 ~ SL_DISP_HEIGHT-1，偏移前）
 * @param  color  像素颜色 (0=灭, 非0=亮)
 * @note   先加上全局偏移，再裁剪至屏幕范围；
 *         使用位操作设置/清除对应 bit，并扩展脏矩形
 */
void sl_disp_draw_pixel(int x, int y, int color) {
    x += g_offset_x;
    y += g_offset_y;
    if (x < 0 || x >= SL_DISP_WIDTH || y < 0 || y >= SL_DISP_HEIGHT) return;

    int byte_idx = (y / 8) * SL_DISP_WIDTH + x;
    uint8_t bit = (uint8_t)(1 << (y & 0x07));

    if (color) {
        draw_buf[byte_idx] |=  bit;
    } else {
        draw_buf[byte_idx] &= ~bit;
    }
    dirty_add_pixel(x, y);
}

/**
 * @brief  填充矩形区域
 * @param  x      矩形左上角 X 坐标（偏移前）
 * @param  y      矩形左上角 Y 坐标（偏移前）
 * @param  w      矩形宽度（像素）
 * @param  h      矩形高度（像素）
 * @param  color  填充颜色 (0=灭, 非0=亮)
 * @note   自动裁剪至屏幕范围；
 *         页对齐行使用 memset 批量写入（快速路径）；
 *         非页对齐行使用位掩码逐列操作
 */
void sl_disp_fill_rect(int x, int y, int w, int h, int color) {
    x += g_offset_x;
    y += g_offset_y;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SL_DISP_WIDTH)  w = SL_DISP_WIDTH - x;
    if (y + h > SL_DISP_HEIGHT) h = SL_DISP_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    int page_start = y / 8;
    int page_end   = (y + h - 1) / 8;
    uint8_t fill = color ? 0xFF : 0x00;

    for (int page = page_start; page <= page_end; page++) {
        int page_y_top = page * 8;
        int page_y_bot = page_y_top + 7;
        int row_start = (y > page_y_top) ? y : page_y_top;
        int row_end   = ((y + h - 1) < page_y_bot) ? (y + h - 1) : page_y_bot;

        if (row_start == page_y_top && row_end == page_y_bot) {
            int base = page * SL_DISP_WIDTH + x;
            if (fill == 0x00) {
                memset(&draw_buf[base], 0, (size_t)w);
            } else {
                memset(&draw_buf[base], 0xFF, (size_t)w);
            }
        } else {
            uint8_t mask = 0;
            for (int r = row_start; r <= row_end; r++) {
                mask |= (uint8_t)(1 << (r & 0x07));
            }
            int base = page * SL_DISP_WIDTH + x;
            for (int dx = 0; dx < w; dx++) {
                if (color) {
                    draw_buf[base + dx] |= mask;
                } else {
                    draw_buf[base + dx] &= ~mask;
                }
            }
        }
    }

    dirty_add_pixel(x, y);
    dirty_add_pixel(x + w - 1, y + h - 1);
}

/**
 * @brief  绘制 1bpp 单色位图
 * @param  x       位图左上角 X 坐标（偏移前）
 * @param  y       位图左上角 Y 坐标（偏移前）
 * @param  w       位图宽度（像素）
 * @param  h       位图高度（像素）
 * @param  bitmap  位图数据指针（每行 ceil(w/8) 字节，MSB 在左）
 * @param  color   前景色 (0=灭, 非0=亮)
 * @note   逐像素调用 sl_disp_draw_pixel，自动处理偏移和裁剪
 */
void sl_disp_draw_bitmap_1bpp(int x, int y, int w, int h,
                              const uint8_t *bitmap, uint8_t color) {
    if (!bitmap || w <= 0 || h <= 0) {
        return;
    }

    int stride = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        const uint8_t *row_ptr = bitmap + row * stride;
        for (int col = 0; col < w; col++) {
            uint8_t bits = row_ptr[col / 8];
            uint8_t mask = (uint8_t)(0x80u >> (col & 0x07));
            if (bits & mask) {
                sl_disp_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief  绘制字符串至显存（通过字体模块代理）
 * @param  x      起始 X 坐标
 * @param  y      起始 Y 坐标
 * @param  str    待绘制的字符串（ASCII）
 * @param  font   字体指针（传入 NULL 使用默认字体）
 * @param  color  文字颜色 (0=灭, 非0=亮)
 * @retval 绘制结束后的 X 坐标
 * @note   实际绘制由 sl_font_draw_internal() 代理，
 *         该函数内部调用 sl_disp_draw_pixel()
 */
uint16_t sl_disp_draw_string(uint16_t x, uint16_t y, const char *str,
                             const void *font, uint8_t color) {
    extern uint16_t sl_font_draw_internal(uint16_t x, uint16_t y, const char *str,
                                          const void *font, uint8_t color);
    return sl_font_draw_internal(x, y, str, font, color);
}

/**
 * @brief  将脏矩形区域刷新到物理屏幕
 *
 * 刷新流程：
 *   1. 若脏区为空，直接返回；
 *   2. 计算脏区覆盖的页范围（每 8 行一页），页对齐后设置硬件窗口；
 *   3. 逐页发送字节数据到 LCD（可选双缓冲 + 异步 DMA）；
 *   4. 刷新完成后重置脏矩形为空。
 */
void sl_disp_flush(void) {
    if (dirty.is_empty) return;

    int x = dirty.x1;
    int y = dirty.y1;
    int w = dirty.x2 - dirty.x1 + 1;
    int h = dirty.y2 - dirty.y1 + 1;

    int page_start = y / 8;
    int page_end   = (y + h - 1) / 8;
    int y_aligned  = page_start * 8;
    int h_aligned  = (page_end - page_start + 1) * 8;

#if SL_USE_PINGPONG_BUF && SL_PORT_USE_ASYNC_TX
    if (sl_hw_tx_busy()) {
        sl_hw_tx_wait();
    }
#endif
    sl_hw_set_window(x, y_aligned, w, h_aligned);

    for (int page = page_start; page <= page_end; page++) {
        uint8_t *src = draw_buf + page * SL_DISP_WIDTH + x;
#if SL_USE_PINGPONG_BUF
        uint8_t *dst = flush_buf + page * SL_DISP_WIDTH + x;
        memcpy(dst, src, (size_t)w);
        src = dst;
#endif
#if SL_USE_PINGPONG_BUF && SL_PORT_USE_ASYNC_TX
        if (page > page_start) {
            sl_hw_tx_wait();
        }
        if (!sl_hw_send_pixels_async(src, w)) {
            sl_hw_send_pixels(src, w);
        }
#else
        sl_hw_send_pixels(src, w);
#endif
    }

    dirty.is_empty = 1;
}

/* ======================== 辅助绘制函数实现 ======================== */

/**
 * @brief  绘制水平线
 * @param  x      起始 X 坐标
 * @param  y      Y 坐标
 * @param  w      线宽（像素）
 * @param  color  颜色 (0=灭, 非0=亮)
 * @note   直接操作页内位掩码，比逐像素调用 draw_pixel 更高效
 */
void sl_disp_draw_hline(int x, int y, int w, int color) {
    if (w <= 0 || y < 0 || y >= SL_DISP_HEIGHT) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > SL_DISP_WIDTH) w = SL_DISP_WIDTH - x;
    if (w <= 0) return;

    int page = y / 8;
    uint8_t bit = (uint8_t)(1 << (y & 0x07));
    int base = page * SL_DISP_WIDTH + x;
    for (int dx = 0; dx < w; dx++) {
        if (color) {
            draw_buf[base + dx] |= bit;
        } else {
            draw_buf[base + dx] &= ~bit;
        }
    }
    dirty_add_pixel(x, y);
    dirty_add_pixel(x + w - 1, y);
}

/**
 * @brief  绘制垂直线
 * @param  x      X 坐标
 * @param  y      起始 Y 坐标
 * @param  h      线高（像素）
 * @param  color  颜色 (0=灭, 非0=亮)
 * @note   逐像素调用 draw_pixel，自动处理跨页情况
 */
void sl_disp_draw_vline(int x, int y, int h, int color) {
    if (h <= 0 || x < 0 || x >= SL_DISP_WIDTH) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > SL_DISP_HEIGHT) h = SL_DISP_HEIGHT - y;
    if (h <= 0) return;

    for (int dy = 0; dy < h; dy++) {
        sl_disp_draw_pixel(x, y + dy, color);
    }
}

/**
 * @brief  绘制反色标题栏（16px 高，y=0）
 * @param  title  标题文本（可为 NULL）
 * @param  font   字体指针
 * @note   背景亮色，文字暗色，形成反色效果；
 *         高度适配 8x16 默认字体
 */
void sl_disp_draw_title_bar(const char *title, const void *font) {
    sl_disp_fill_rect(0, 0, SL_DISP_WIDTH, 16, 1);
    sl_disp_draw_string(2, 0, title ? title : "", font, 0);
}

/**
 * @brief  绘制反色状态栏（16px 高）
 * @param  y      状态栏 Y 坐标
 * @param  text   状态文本（可为 NULL）
 * @param  font   字体指针
 * @note   背景亮色，文字暗色，形成反色效果
 */
void sl_disp_draw_status_bar(int y, const char *text, const void *font) {
    sl_disp_fill_rect(0, y, SL_DISP_WIDTH, 16, 1);
    sl_disp_draw_string(2, y, text ? text : "", font, 0);
}

/**
 * @brief  设置全局绘制偏移量
 * @param  dx  X 方向偏移（像素），正值向右
 * @param  dy  Y 方向偏移（像素），正值向下
 * @note   后续所有绘制操作的坐标都会加上此偏移；
 *         用于页面过渡动画中的滑动效果
 */
void sl_disp_set_offset(int dx, int dy) {
    g_offset_x = dx;
    g_offset_y = dy;
}
