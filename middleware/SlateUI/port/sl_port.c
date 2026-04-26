#include "sl_port.h"
#include "../../../bsp/st7920/at32f415_st7920_port.h"
#include "../../../project/inc/wk_system.h"
#include <string.h>

typedef struct
{
  int x;
  int y;
  int w;
  int h;
  int bytes_expected;
  int bytes_received;
  uint8_t page_buf[SL_DISP_WIDTH * SL_DISP_HEIGHT / 8];
} sl_port_state_t;

static sl_port_state_t g_sl_port_state;

static int clip_i32(int v, int min_v, int max_v)
{
  if(v < min_v) { return min_v; }
  if(v > max_v) { return max_v; }
  return v;
}

void sl_port_init(void)
{
  memset(&g_sl_port_state, 0, sizeof(g_sl_port_state));
  (void)at32_st7920_init();
}

void sl_hw_set_window(int x, int y, int w, int h)
{
  int pages;
  g_sl_port_state.x = clip_i32(x, 0, SL_DISP_WIDTH - 1);
  g_sl_port_state.y = clip_i32(y, 0, SL_DISP_HEIGHT - 1);
  g_sl_port_state.w = clip_i32(w, 0, SL_DISP_WIDTH);
  g_sl_port_state.h = clip_i32(h, 0, SL_DISP_HEIGHT);
  pages = (g_sl_port_state.h + 7) / 8;
  g_sl_port_state.bytes_expected = g_sl_port_state.w * pages;
  g_sl_port_state.bytes_received = 0;
}

void sl_hw_send_pixels(const uint8_t *data, int len)
{
  int remaining;
  int copy_len;
  int page_start;
  int pages;
  int i;
  if((data == 0) || (len <= 0))
  {
    return;
  }

  remaining = g_sl_port_state.bytes_expected - g_sl_port_state.bytes_received;
  copy_len = (len < remaining) ? len : remaining;
  if(copy_len <= 0)
  {
    return;
  }

  page_start = g_sl_port_state.y / 8;
  pages = (g_sl_port_state.h + 7) / 8;
  (void)pages;

  for(i = 0; i < copy_len; ++i)
  {
    int stream_index;
    int page_off;
    int x_off;
    int dst_index;
    stream_index = g_sl_port_state.bytes_received + i;
    if(g_sl_port_state.w <= 0)
    {
      break;
    }
    page_off = stream_index / g_sl_port_state.w;
    x_off = stream_index % g_sl_port_state.w;
    dst_index = (page_start + page_off) * SL_DISP_WIDTH + g_sl_port_state.x + x_off;
    if((dst_index >= 0) && (dst_index < (SL_DISP_WIDTH * SL_DISP_HEIGHT / 8)))
    {
      g_sl_port_state.page_buf[dst_index] = data[i];
    }
  }

  g_sl_port_state.bytes_received += copy_len;
  if(g_sl_port_state.bytes_received >= g_sl_port_state.bytes_expected)
  {
    (void)at32_st7920_flush_page_buffer(g_sl_port_state.page_buf);
  }
}

void sl_port_input_init(void)
{
}

void sl_port_poll_input(void)
{
}

void sl_port_enable_key_interrupts(void)
{
  sl_port_input_init();
}

int sl_hw_send_pixels_async(const uint8_t *data, int len)
{
  sl_hw_send_pixels(data, len);
  return 1;
}

int sl_hw_tx_busy(void)
{
  return 0;
}

void sl_hw_tx_wait(void)
{
}
