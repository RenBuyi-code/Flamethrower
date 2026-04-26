#ifndef HAL_IF_I_DMX_H
#define HAL_IF_I_DMX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  void *ctx;
  bool (*poll_byte)(void *ctx, uint8_t *byte, bool *is_break);
} i_dmx_t;

#endif
