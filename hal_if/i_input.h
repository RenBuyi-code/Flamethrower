#ifndef HAL_IF_I_INPUT_H
#define HAL_IF_I_INPUT_H

#include <stdbool.h>
#include "i_types.h"

typedef struct
{
  void *ctx;
  bool (*read)(void *ctx, input_id_t id);
} i_input_t;

#endif
