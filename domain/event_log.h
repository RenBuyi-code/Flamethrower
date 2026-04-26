#ifndef DOMAIN_EVENT_LOG_H
#define DOMAIN_EVENT_LOG_H

#include <stdint.h>

#define EVENT_LOG_CAPACITY 64U

typedef struct
{
  uint16_t code;
  uint32_t timestamp_ms;
} event_item_t;

typedef struct
{
  event_item_t data[EVENT_LOG_CAPACITY];
  uint16_t head;
} event_log_t;

void event_log_init(event_log_t *log);
void event_log_push(event_log_t *log, uint16_t code, uint32_t ts_ms);

#endif
