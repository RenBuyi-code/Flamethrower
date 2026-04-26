#include "event_log.h"

void event_log_init(event_log_t *log)
{
  uint16_t i;
  if(log == 0)
  {
    return;
  }
  log->head = 0U;
  for(i = 0U; i < EVENT_LOG_CAPACITY; ++i)
  {
    log->data[i].code = 0U;
    log->data[i].timestamp_ms = 0U;
  }
}

void event_log_push(event_log_t *log, uint16_t code, uint32_t ts_ms)
{
  uint16_t index;
  if(log == 0)
  {
    return;
  }
  index = (uint16_t)(log->head % EVENT_LOG_CAPACITY);
  log->data[index].code = code;
  log->data[index].timestamp_ms = ts_ms;
  log->head = (uint16_t)(log->head + 1U);
}
