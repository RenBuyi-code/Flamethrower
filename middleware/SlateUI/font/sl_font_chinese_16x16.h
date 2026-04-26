/**
 * @file    sl_font_chinese_16x16.h
 * @brief   16x16 中文 GB2312 常用字子集数据声明
 *
 * 本文件声明 16x16 点阵中文字库：
 *   · 每个汉字占 32 字节（16 列 × 每列 2 字节）
 *   · 编码格式：列优先、阴码（1=亮）、LSB 低位在前
 *   · 每列 2 字节：低字节=上半部(row0~7)，高字节=下半部(row8~15)
 *   · 数据存储在 Flash 中（const），零 RAM 占用。
 *
 * 取模工具设置（PCtoLCD2002）：
 *   · 阴码-1亮0灭
 *   · 逐列（列优先）
 *   · LSB-低位在前
 *   · 字体：宋体/黑体 16×16
 *
 * 使用方式：
 *   通过 sl_chinese_index() 将汉字转为索引，
 *   再由 sl_font_draw_internal() + &sl_font_chinese 绘制。
 */

#ifndef SL_FONT_CHINESE_16X16_H
#define SL_FONT_CHINESE_16X16_H

#include <stdint.h>
#include "sl_font.h"

/** @brief 支持的汉字+ASCII 16x16字符总数 */
#define SL_CHINESE_GLYPH_COUNT  170

/**
 * @brief  16x16 中文字模数据表
 *
 * 每字符 32 字节：col0_lo, col0_hi, col1_lo, col1_hi, ..., col15_hi
 * 索引方式：sl_chinese_glyphs[index]，index 由 sl_chinese_index() 获取
 */
extern const uint8_t sl_chinese_glyphs[SL_CHINESE_GLYPH_COUNT][32];

/**
 * @brief  通过 Unicode 码点查找汉字在字模表中的索引
 * @param  unicode  Unicode 码点（如 U+4E2D = 0x4E2D）
 * @retval 索引值 (0 ~ SL_CHINESE_GLYPH_COUNT-1)
 * @retval 0xFFFF 表示未找到该汉字
 */
uint16_t sl_chinese_index(uint16_t unicode);

/** @brief 16x16 中文字体对象（兼容 ASCII + UTF-8 中文） */
extern sl_Font sl_font_chinese;

#endif
