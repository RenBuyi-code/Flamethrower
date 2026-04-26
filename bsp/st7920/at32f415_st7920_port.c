/**
 * @file    at32f415_st7920_port.c
 * @brief   ST7920 LCD显示屏端口实现
 *
 * ST7920显示屏驱动端口层，负责：
 *   - 初始化ST7920 LCD显示屏
 *   - 提供GPIO接口函数
 *   - 实现页面缓冲区到显示图像的转换
 *   - 提供延时和调试打印接口
 *
 * 设计思路：
 *   - 使用软件SPI接口（CS、SCLK、SID）
 *   - 支持像素极性反转（适用于某些LCD模块）
 *   - 将页面缓冲区转换为ST7920可显示的图像格式
 *   - 与其他模块的关系：
 *     - driver_st7920.c：使用通用ST7920驱动
 *     - app/log_rtt.h：用于日志输出
 */

#include "at32f415_st7920_port.h"
#include "../../project/inc/at32f415_wk_config.h"
#include "../../project/inc/at32f415_conf.h"
#include "../../project/inc/wk_system.h"
#include "../../app/log_rtt.h"
#include "src/driver_st7920.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** @brief ST7920芯片选择端口和引脚定义 */
#define ST7920_CS_PORT LCD_CS_GPIO_PORT
#define ST7920_CS_PIN LCD_CS_PIN
/** @brief ST7920时钟端口和引脚定义 */
#define ST7920_SCLK_PORT LCD_SCK_GPIO_PORT
#define ST7920_SCLK_PIN LCD_SCK_PIN
/** @brief ST7920数据端口和引脚定义 */
#define ST7920_SID_PORT LCD_SID_GPIO_PORT
#define ST7920_SID_PIN LCD_SID_PIN
/** @brief 是否反转单色像素极性：1=反转, 0=保持原样 */
#define ST7920_PIXEL_INVERT 1

/** @brief ST7920全局句柄 */
static st7920_handle_t g_st7920;
/** @brief ST7920初始化标志 */
static uint8_t g_st7920_inited = 0U;
/** @brief ST7920图像缓冲区（128x64像素 = 1024字节） */
static uint8_t g_st7920_img[1024];

/**
 * @brief   ST7920 CS引脚初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_cs_gpio_init(void)
{
  return 0;
}

/**
 * @brief   ST7920 CS引脚反初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_cs_gpio_deinit(void)
{
  return 0;
}

/**
 * @brief   ST7920 CS引脚写入接口
 *
 * @param[in] value  写入的值（0或非0）
 * @return    状态代码（0=成功）
 */
static uint8_t st7920_interface_cs_gpio_write(uint8_t value)
{
  if (value != 0U)
  {
    gpio_bits_set(ST7920_CS_PORT, ST7920_CS_PIN);
  }
  else
  {
    gpio_bits_reset(ST7920_CS_PORT, ST7920_CS_PIN);
  }
  return 0;
}

/**
 * @brief   ST7920 SCLK引脚初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_sclk_gpio_init(void)
{
  return 0;
}

/**
 * @brief   ST7920 SCLK引脚反初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_sclk_gpio_deinit(void)
{
  return 0;
}

/**
 * @brief   ST7920 SCLK引脚写入接口
 *
 * @param[in] value  写入的值（0或非0）
 * @return    状态代码（0=成功）
 */
static uint8_t st7920_interface_sclk_gpio_write(uint8_t value)
{
  if (value != 0U)
  {
    gpio_bits_set(ST7920_SCLK_PORT, ST7920_SCLK_PIN);
  }
  else
  {
    gpio_bits_reset(ST7920_SCLK_PORT, ST7920_SCLK_PIN);
  }
  return 0;
}

/**
 * @brief   ST7920 SID引脚初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_sid_gpio_init(void)
{
  return 0;
}

/**
 * @brief   ST7920 SID引脚反初始化接口
 *
 * @return    状态代码（0=成功）
 *
 * @note    本实现中GPIO已在系统初始化时配置，此函数为空
 */
static uint8_t st7920_interface_sid_gpio_deinit(void)
{
  return 0;
}

/**
 * @brief   ST7920 SID引脚写入接口
 *
 * @param[in] value  写入的值（0或非0）
 * @return    状态代码（0=成功）
 */
static uint8_t st7920_interface_sid_gpio_write(uint8_t value)
{
  if (value != 0U)
  {
    gpio_bits_set(ST7920_SID_PORT, ST7920_SID_PIN);
  }
  else
  {
    gpio_bits_reset(ST7920_SID_PORT, ST7920_SID_PIN);
  }
  return 0;
}

/**
 * @brief   ST7920延时接口（毫秒）
 *
 * @param[in] ms  延时毫秒数
 */
static void st7920_interface_delay_ms(uint32_t ms)
{
  wk_delay_ms(ms);
}

/**
 * @brief   ST7920延时接口（微秒）
 *
 * @param[in] us  延时微秒数
 */
static void st7920_interface_delay_us(uint32_t us)
{
  wk_delay_us(us);
}

/**
 * @brief   ST7920调试打印接口
 *
 * @param[in] fmt  格式化字符串
 * @param[in] ...  可变参数列表
 *
 * 将调试信息通过RTT日志输出
 */
static void st7920_interface_debug_print(const char *const fmt, ...)
{
  va_list args;
  char buf[128];
  va_start(args, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  APP_LOGW("st7920: %s", buf);
}

/**
 * @brief   连接所有ST7920接口
 *
 * @param[out] h  ST7920句柄指针
 *
 * 将所有硬件接口函数绑定到ST7920句柄
 */
static void st7920_link_all(st7920_handle_t *h)
{
  DRIVER_ST7920_LINK_INIT(h, st7920_handle_t);
  DRIVER_ST7920_LINK_CS_GPIO_INIT(h, st7920_interface_cs_gpio_init);
  DRIVER_ST7920_LINK_CS_GPIO_DEINIT(h, st7920_interface_cs_gpio_deinit);
  DRIVER_ST7920_LINK_CS_GPIO_WRITE(h, st7920_interface_cs_gpio_write);
  DRIVER_ST7920_LINK_SCLK_GPIO_INIT(h, st7920_interface_sclk_gpio_init);
  DRIVER_ST7920_LINK_SCLK_GPIO_DEINIT(h, st7920_interface_sclk_gpio_deinit);
  DRIVER_ST7920_LINK_SCLK_GPIO_WRITE(h, st7920_interface_sclk_gpio_write);
  DRIVER_ST7920_LINK_SID_GPIO_INIT(h, st7920_interface_sid_gpio_init);
  DRIVER_ST7920_LINK_SID_GPIO_DEINIT(h, st7920_interface_sid_gpio_deinit);
  DRIVER_ST7920_LINK_SID_GPIO_WRITE(h, st7920_interface_sid_gpio_write);
  DRIVER_ST7920_LINK_DELAY_MS(h, st7920_interface_delay_ms);
  DRIVER_ST7920_LINK_DELAY_US(h, st7920_interface_delay_us);
  DRIVER_ST7920_LINK_DEBUG_PRINT(h, st7920_interface_debug_print);
}

/**
 * @brief   将页面缓冲区转换为ST7920图像格式
 *
 * @param[in] page_buffer  页面缓冲区指针（128x64像素，每字节8像素垂直排列）
 * @param[out] img        输出的图像缓冲区（1024字节）
 *
 * 转换说明：
 *   - 输入：页面缓冲区格式，每字节垂直排列8个像素
 *   - 输出：ST7920格式，按字节水平排列
 *   - 坐标系：x从左到右，y从上到下
 *
 * 转换算法：
 *   1. 遍历所有像素(x,y)
 *   2. 从源缓冲区读取对应字节和位掩码
 *   3. 计算目标缓冲区的字节位置和位偏移
 *   4. 设置或清除目标位
 */
static void st7920_convert_page_buffer_to_img(const uint8_t *page_buffer, uint8_t *img)
{
  uint16_t x;
  uint16_t y;
  memset(img, 0, 1024U);

  for (x = 0U; x < 128U; ++x)
  {
    for (y = 0U; y < 64U; ++y)
    {
      uint32_t src_index;
      uint8_t src_mask;
      uint32_t l;
      uint32_t m;
      uint32_t n;
      src_index = ((uint32_t)(y / 8U) * 128U) + x;
      src_mask = (uint8_t)(1U << (y & 0x07U));
      if ((page_buffer[src_index] & src_mask) != 0U)
      {
        l = (uint32_t)x * 64U + y;
        m = l / 8U;
        n = l % 8U;
        img[m] |= (uint8_t)(1U << (7U - n));
      }
    }
  }
}

/**
 * @brief   初始化ST7920 LCD显示屏
 *
 * @return    是否初始化成功
 *
 * 初始化流程：
 *   1. 检查是否已初始化
 *   2. 连接所有接口
 *   3. 调用驱动初始化
 *   4. 设置为8位总线模式
 *   5. 启用显示
 *   6. 设置为扩展命令模式（支持图形显示）
 *   7. 清屏
 */
bool at32_st7920_init(void)
{
  uint8_t res;

  if (g_st7920_inited != 0U)
  {
    return true;
  }

  st7920_link_all(&g_st7920);
  res = st7920_init(&g_st7920);
  if (res != 0U)
  {
    APP_LOGE("st7920 init failed: %u", (unsigned)res);
    return false;
  }

  (void)st7920_set_function(&g_st7920, ST7920_INTERFACE_BUS_BIT_8, ST7920_COMMAND_MODE_BASIC);
  (void)st7920_set_display_control(&g_st7920, ST7920_BOOL_TRUE, ST7920_BOOL_FALSE, ST7920_BOOL_FALSE);
  (void)st7920_set_extended_function(&g_st7920, ST7920_INTERFACE_BUS_BIT_8, ST7920_COMMAND_MODE_EXTENDED, ST7920_BOOL_TRUE);
  (void)st7920_set_scroll_address(&g_st7920, 0U);
  (void)st7920_fill_rect(&g_st7920, 0U, 0U, 127U, 63U, 0U);
  g_st7920_inited = 1U;
  APP_LOGI("st7920 init ok");
  return true;
}

/**
 * @brief   刷新页面缓冲区到LCD显示屏
 *
 * @param[in] page_buffer_128x64  页面缓冲区指针（128x64像素）
 * @return    是否刷新成功
 *
 * 操作流程：
 *   1. 检查参数有效性
 *   2. 必要时初始化显示屏
 *   3. 转换页面缓冲区到图像格式
 *   4. 可选：反转像素极性
 *   5. 发送到显示屏显示
 */
bool at32_st7920_flush_page_buffer(const uint8_t *page_buffer_128x64)
{
  uint8_t res;
  if (page_buffer_128x64 == 0)
  {
    return false;
  }
  if (g_st7920_inited == 0U)
  {
    if (at32_st7920_init() == false)
    {
      return false;
    }
  }

  st7920_convert_page_buffer_to_img(page_buffer_128x64, g_st7920_img);
#if (ST7920_PIXEL_INVERT == 1)
  {
    uint32_t i;
    for (i = 0U; i < (uint32_t)sizeof(g_st7920_img); ++i)
    {
      g_st7920_img[i] = (uint8_t)~g_st7920_img[i];
    }
  }
#endif
  res = st7920_draw_compress_picture(&g_st7920, 0U, 0U, 127U, 63U, g_st7920_img);
  if (res != 0U)
  {
    APP_LOGE("st7920 flush failed: %u", (unsigned)res);
    return false;
  }
  return true;
}
