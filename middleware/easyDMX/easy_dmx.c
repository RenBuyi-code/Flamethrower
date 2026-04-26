#include "easy_dmx.h"
#include <string.h>

static bool is_power_of_two(size_t value)
{
  return (value != 0U) && ((value & (value - 1U)) == 0U);
}

static void fifo_write_byte(edmx_fifo_t *fifo, uint8_t value)
{
  fifo->buffer[fifo->head & fifo->mask] = value;
  fifo->head++;
}

static uint8_t fifo_read_byte(edmx_fifo_t *fifo)
{
  uint8_t value;
  value = fifo->buffer[fifo->tail & fifo->mask];
  fifo->tail++;
  return value;
}

static void parser_reset(edmx_rx_t *rx)
{
  rx->parser_slot_count = 0U;
  rx->parser_start_code = 0U;
  rx->parser_has_start_code = false;
  rx->parser_overflow = false;
}

static void finalize_frame(edmx_rx_t *rx, uint32_t now_ms)
{
  if(rx->parser_has_start_code == false)
  {
    return;
  }

  if(rx->parser_start_code != 0U)
  {
    rx->stats.frames_nonzero_start++;
    return;
  }

  if(rx->parser_slot_count == 0U)
  {
    rx->stats.frames_short++;
    return;
  }

  if(rx->parser_overflow)
  {
    rx->stats.frames_long++;
  }

  rx->latest.start_code = rx->parser_start_code;
  rx->latest.slot_count = (uint16_t)rx->parser_slot_count;
  memcpy(rx->latest.channels, rx->parser_channels, sizeof(rx->parser_channels));
  rx->latest.sequence++;
  rx->latest.tick_ms = now_ms;
  rx->latest.valid = true;

  rx->last_frame_tick_ms = now_ms;
  rx->stats.frames_ok++;
}

static bool pop_event(edmx_rx_t *rx, edmx_event_t *evt)
{
  if((rx == 0) || (evt == 0))
  {
    return false;
  }

  if(edmx_fifo_used(&rx->fifo) < sizeof(edmx_event_t))
  {
    return false;
  }

  evt->flags = fifo_read_byte(&rx->fifo);
  evt->byte = fifo_read_byte(&rx->fifo);
  return true;
}

bool edmx_fifo_init(edmx_fifo_t *fifo, uint8_t *storage, size_t size_bytes)
{
  if((fifo == 0) || (storage == 0) || (size_bytes < 2U) || (is_power_of_two(size_bytes) == false))
  {
    return false;
  }

  fifo->head = 0U;
  fifo->tail = 0U;
  fifo->mask = (uint32_t)(size_bytes - 1U);
  fifo->buffer = storage;
  fifo->overruns = 0U;
  memset(storage, 0, size_bytes);
  return true;
}

size_t edmx_fifo_used(const edmx_fifo_t *fifo)
{
  if(fifo == 0)
  {
    return 0U;
  }
  return (size_t)(fifo->head - fifo->tail);
}

size_t edmx_fifo_free(const edmx_fifo_t *fifo)
{
  if(fifo == 0)
  {
    return 0U;
  }
  return (size_t)(fifo->mask + 1U) - edmx_fifo_used(fifo);
}

bool edmx_rx_init(edmx_rx_t *rx, uint8_t *fifo_storage, size_t fifo_size_bytes, uint32_t online_timeout_ms)
{
  if((rx == 0) || (fifo_storage == 0))
  {
    return false;
  }

  memset(rx, 0, sizeof(*rx));
  if(edmx_fifo_init(&rx->fifo, fifo_storage, fifo_size_bytes) == false)
  {
    return false;
  }

  rx->online_timeout_ms = online_timeout_ms;
  parser_reset(rx);
  return true;
}

bool edmx_rx_push_event(edmx_rx_t *rx, const edmx_event_t *evt)
{
  if((rx == 0) || (evt == 0))
  {
    return false;
  }

  if(edmx_fifo_free(&rx->fifo) < sizeof(edmx_event_t))
  {
    rx->fifo.overruns++;
    rx->stats.fifo_overruns = rx->fifo.overruns;
    rx->stats.bytes_dropped += (uint32_t)sizeof(edmx_event_t);
    return false;
  }

  fifo_write_byte(&rx->fifo, evt->flags);
  fifo_write_byte(&rx->fifo, evt->byte);
  return true;
}

void edmx_rx_process(edmx_rx_t *rx, uint32_t now_ms)
{
  edmx_event_t evt;

  if(rx == 0)
  {
    return;
  }

  while(pop_event(rx, &evt))
  {
    if((evt.flags & EDMX_EVENT_FLAG_BREAK) != 0U)
    {
      rx->stats.breaks_seen++;
      finalize_frame(rx, now_ms);
      parser_reset(rx);
      continue;
    }

    if(rx->parser_has_start_code == false)
    {
      rx->parser_has_start_code = true;
      rx->parser_start_code = evt.byte;
      continue;
    }

    if(rx->parser_slot_count < EDMX_UNIVERSE_SIZE)
    {
      if(rx->parser_start_code == 0U)
      {
        rx->parser_channels[rx->parser_slot_count] = evt.byte;
      }
      rx->parser_slot_count++;
    }
    else
    {
      rx->parser_overflow = true;
    }
  }
}

bool edmx_rx_copy_latest(const edmx_rx_t *rx, edmx_frame_t *out)
{
  if((rx == 0) || (out == 0) || (rx->latest.valid == false))
  {
    return false;
  }

  *out = rx->latest;
  return true;
}

bool edmx_rx_is_online(const edmx_rx_t *rx, uint32_t now_ms)
{
  if((rx == 0) || (rx->latest.valid == false))
  {
    return false;
  }

  if((now_ms - rx->last_frame_tick_ms) > rx->online_timeout_ms)
  {
    return false;
  }

  return true;
}

const edmx_stats_t *edmx_rx_get_stats(const edmx_rx_t *rx)
{
  if(rx == 0)
  {
    return 0;
  }
  return &rx->stats;
}
