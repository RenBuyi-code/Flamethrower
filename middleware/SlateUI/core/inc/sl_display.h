/**
 * @file    sl_display.h
 * @brief   SlateUI 显示缓冲与绘制接口
 *
 * 本模块管理一块静态显存缓冲区 (draw_buf)，所有绘制操作先写入缓冲区，
 * 再由 sl_disp_flush() 将脏矩形区域刷新到物理屏幕。
 *
 * 显存组织（兼容 SSD1306 页寻址）：
 *   byte_idx = (y / 8) * SL_DISP_WIDTH + x
 *   每字节的 bit(y % 8) 代表该像素的亮灭。
 *
 * 支持全局绘制偏移 (sl_disp_set_offset)，用于页面过渡动画。
 */

#ifndef SL_DISPLAY_H
#define SL_DISPLAY_H

#include <stdint.h>

/* ======================== 屏幕尺寸配置 ======================== */

/** @brief 屏幕宽度（像素），可在编译选项中覆盖 */
#ifndef SL_DISP_WIDTH
#define SL_DISP_WIDTH 128
#endif

/** @brief 屏幕高度（像素），可在编译选项中覆盖 */
#ifndef SL_DISP_HEIGHT
#define SL_DISP_HEIGHT 64
#endif

/** @brief 显存缓冲区大小（字节） */
#define SL_DISP_BUF_SIZE ((SL_DISP_WIDTH * SL_DISP_HEIGHT) / 8)

/**
 * @brief  是否启用双缓冲 (ping-pong)
 * @note   启用后 flush 使用独立缓冲区，避免 DMA 读取冲突；
 *         需要额外 SL_DISP_BUF_SIZE 字节 RAM
 */
#ifndef SL_USE_PINGPONG_BUF
#define SL_USE_PINGPONG_BUF 0
#endif

/* ======================== 核心绘制接口 ======================== */

/**
 * @brief  初始化显存缓冲区与脏矩形
 * @note   清空整块缓冲区并标记全屏脏区，下次 flush 将发送全部像素
 */
void     sl_disp_init(void);

/**
 * @brief  获取显存缓冲区首地址
 * @retval 指向 draw_buf 的指针
 */
uint8_t *sl_disp_get_buffer(void);

/**
 * @brief  绘制单个像素
 * @param  x      像素 X 坐标 (0 ~ SL_DISP_WIDTH-1)
 * @param  y      像素 Y 坐标 (0 ~ SL_DISP_HEIGHT-1)
 * @param  color  像素颜色 (0=灭, 非0=亮)
 */
void     sl_disp_draw_pixel(int x, int y, int color);

/**
 * @brief  填充矩形区域
 * @param  x      矩形左上角 X 坐标
 * @param  y      矩形左上角 Y 坐标
 * @param  w      矩形宽度（像素）
 * @param  h      矩形高度（像素）
 * @param  color  填充颜色 (0=灭, 非0=亮)
 * @note   自动裁剪至屏幕范围；页对齐时使用 memset 批量写入
 */
void     sl_disp_fill_rect(int x, int y, int w, int h, int color);

/**
 * @brief  绘制 1bpp 单色位图
 * @param  x       位图左上角 X 坐标
 * @param  y       位图左上角 Y 坐标
 * @param  w       位图宽度（像素）
 * @param  h       位图高度（像素）
 * @param  bitmap  位图数据指针（每行 ceil(w/8) 字节，MSB 在左）
 * @param  color   前景色 (0=灭, 非0=亮)
 */
void     sl_disp_draw_bitmap_1bpp(int x, int y, int w, int h,
                                  const uint8_t *bitmap, uint8_t color);

/**
 * @brief  绘制字符串
 * @param  x      起始 X 坐标
 * @param  y      起始 Y 坐标
 * @param  str    待绘制的字符串（ASCII）
 * @param  font   字体指针（传入 NULL 使用默认字体）
 * @param  color  文字颜色 (0=灭, 非0=亮)
 * @retval 绘制结束后的 X 坐标
 */
uint16_t sl_disp_draw_string(uint16_t x, uint16_t y, const char *str,
                             const void *font, uint8_t color);

/**
 * @brief  将脏矩形区域刷新到物理屏幕
 * @note   通过 sl_hw_set_window / sl_hw_send_pixels 发送数据；
 *         刷新完成后自动重置脏矩形为空
 */
void     sl_disp_flush(void);

/* ======================== 绘制偏移与辅助 ======================== */

/**
 * @brief  设置全局绘制偏移量
 * @param  dx  X 方向偏移（像素），正值向右
 * @param  dy  Y 方向偏移（像素），正值向下
 * @note   后续所有绘制操作的坐标都会加上此偏移；
 *         用于页面过渡动画中的滑动效果
 */
void     sl_disp_set_offset(int dx, int dy);

/**
 * @brief  绘制水平线
 * @param  x      起始 X 坐标
 * @param  y      Y 坐标
 * @param  w      线宽（像素）
 * @param  color  颜色 (0=灭, 非0=亮)
 */
void     sl_disp_draw_hline(int x, int y, int w, int color);

/**
 * @brief  绘制垂直线
 * @param  x      X 坐标
 * @param  y      起始 Y 坐标
 * @param  h      线高（像素）
 * @param  color  颜色 (0=灭, 非0=亮)
 */
void     sl_disp_draw_vline(int x, int y, int h, int color);

/**
 * @brief  绘制反色标题栏（16px 高，适配 8x16 默认字体）
 * @param  title  标题文本（可为 NULL）
 * @param  font   字体指针
 * @note   背景亮色，文字暗色
 */
void     sl_disp_draw_title_bar(const char *title, const void *font);

/**
 * @brief  绘制反色状态栏（16px 高，适配 8x16 默认字体）
 * @param  y      状态栏 Y 坐标
 * @param  text   状态文本（可为 NULL）
 * @param  font   字体指针
 * @note   背景亮色，文字暗色
 */
void     sl_disp_draw_status_bar(int y, const char *text, const void *font);

/* ======================== 硬件抽象层（port 层实现） ======================== */

/**
 * @brief  设置 LCD 更新窗口（port 层实现）
 * @param  x  窗口左上角 X
 * @param  y  窗口左上角 Y
 * @param  w  窗口宽度
 * @param  h  窗口高度
 */
extern void sl_hw_set_window(int x, int y, int w, int h);

/**
 * @brief  发送像素数据到 LCD（port 层实现）
 * @param  data  像素数据指针
 * @param  len   数据长度（字节）
 */
extern void sl_hw_send_pixels(const uint8_t *data, int len);

#endif
