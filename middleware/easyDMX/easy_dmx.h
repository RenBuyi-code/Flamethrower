#ifndef MIDDLEWARE_EASY_DMX_H
#define MIDDLEWARE_EASY_DMX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EDMX_UNIVERSE_SIZE                512U
#define EDMX_EVENT_FLAG_BREAK             0x01U

typedef struct
{
  uint8_t byte;
  uint8_t flags;
} edmx_event_t;

typedef struct
{
  uint32_t head;
  uint32_t tail;
  uint32_t mask;
  uint8_t *buffer;
  uint32_t overruns;
} edmx_fifo_t;

typedef struct
{
  uint8_t start_code;
  uint16_t slot_count;
  uint8_t channels[EDMX_UNIVERSE_SIZE];
  uint32_t sequence;
  uint32_t tick_ms;
  bool valid;
} edmx_frame_t;

typedef struct
{
  uint32_t breaks_seen;
  uint32_t frames_ok;
  uint32_t frames_short;
  uint32_t frames_long;
  uint32_t frames_nonzero_start;
  uint32_t fifo_overruns;
  uint32_t bytes_dropped;
} edmx_stats_t;

typedef struct
{
  edmx_fifo_t fifo;
  edmx_frame_t latest;
  edmx_stats_t stats;
  uint32_t online_timeout_ms;
  uint32_t last_frame_tick_ms;
  uint32_t parser_slot_count;
  uint8_t parser_start_code;
  bool parser_has_start_code;
  bool parser_overflow;
  uint8_t parser_channels[EDMX_UNIVERSE_SIZE];
} edmx_rx_t;

bool edmx_fifo_init(edmx_fifo_t *fifo, uint8_t *storage, size_t size_bytes);
size_t edmx_fifo_used(const edmx_fifo_t *fifo);
size_t edmx_fifo_free(const edmx_fifo_t *fifo);
bool edmx_rx_init(edmx_rx_t *rx, uint8_t *fifo_storage, size_t fifo_size_bytes, uint32_t online_timeout_ms);
bool edmx_rx_push_event(edmx_rx_t *rx, const edmx_event_t *evt);
void edmx_rx_process(edmx_rx_t *rx, uint32_t now_ms);
bool edmx_rx_copy_latest(const edmx_rx_t *rx, edmx_frame_t *out);
bool edmx_rx_is_online(const edmx_rx_t *rx, uint32_t now_ms);
const edmx_stats_t *edmx_rx_get_stats(const edmx_rx_t *rx);

#endif
