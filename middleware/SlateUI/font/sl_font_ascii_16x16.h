/**
 * @file    sl_font_ascii_16x16.h
 * @brief   8x16 ASCII 点阵字库数据声明
 *
 * 本文件声明 95 个可打印 ASCII 字符（0x20~0x7E）的 8x16 点阵数据：
 *   · 每个字符占 16 字节（8 列 × 每列 2 字节）
 *   · 编码格式：列优先、阴码（1=亮）、LSB 低位在前
 *   · 每列 2 字节：低字节=上半部(row0~7)，高字节=下半部(row8~15)
 *   · 数据存储在 Flash 中（const），零 RAM 占用。
 *   · 索引方式：sl_ascii_8x16_data[char - 0x20]
 *
 * 取模工具设置（PCtoLCD2002）：
 *   · 阴码-1亮0灭
 *   · 逐列（列优先）
 *   · LSB-低位在前
 */

#ifndef SL_FONT_ASCII_8X16_H
#define SL_FONT_ASCII_8X16_H

#include <stdint.h>

/** @brief 95 个可打印 ASCII 字符的 8x16 点阵数据（每字符 16 字节，列优先） */
extern const uint8_t sl_ascii_8x16_data[95][16];

/** @brief 8x16 ASCII 字符绘制函数 */
uint16_t ascii_8x16_draw(uint16_t x, uint16_t y, const char *str,
                          const void *font, uint8_t color);

#endif
