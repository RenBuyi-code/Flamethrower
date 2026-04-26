#include "at32f415_st7920_port.h"
#include "../../project/inc/at32f415_wk_config.h"
#include "../../project/inc/at32f415_conf.h"
#include "../../project/inc/wk_system.h"
#include "../../app/log_rtt.h"
#include "src/driver_st7920.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ST7920_CS_PORT LCD_CS_GPIO_PORT
#define ST7920_CS_PIN LCD_CS_PIN
#define ST7920_SCLK_PORT LCD_SCK_GPIO_PORT
#define ST7920_SCLK_PIN LCD_SCK_PIN
#define ST7920_SID_PORT LCD_SID_GPIO_PORT
#define ST7920_SID_PIN LCD_SID_PIN
/* 1: invert monochrome pixel polarity, 0: keep as-is */
#define ST7920_PIXEL_INVERT 1

static st7920_handle_t g_st7920;
static uint8_t g_st7920_inited = 0U;
static uint8_t g_st7920_img[1024];

static uint8_t st7920_interface_cs_gpio_init(void)
{
  return 0;
}

static uint8_t st7920_interface_cs_gpio_deinit(void)
{
  return 0;
}

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

static uint8_t st7920_interface_sclk_gpio_init(void)
{
  return 0;
}

static uint8_t st7920_interface_sclk_gpio_deinit(void)
{
  return 0;
}

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

static uint8_t st7920_interface_sid_gpio_init(void)
{
  return 0;
}

static uint8_t st7920_interface_sid_gpio_deinit(void)
{
  return 0;
}

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

static void st7920_interface_delay_ms(uint32_t ms)
{
  wk_delay_ms(ms);
}

static void st7920_interface_delay_us(uint32_t us)
{
  wk_delay_us(us);
}

static void st7920_interface_debug_print(const char *const fmt, ...)
{
  va_list args;
  char buf[128];
  va_start(args, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  APP_LOGW("st7920: %s", buf);
}

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
