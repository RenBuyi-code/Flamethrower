#ifndef HAL_IF_I_ACTUATOR_H
#define HAL_IF_I_ACTUATOR_H

#include "i_types.h"

typedef struct
{
  void *ctx;
  void (*apply)(void *ctx, const actuator_output_t *out);
} i_actuator_t;

#endif
