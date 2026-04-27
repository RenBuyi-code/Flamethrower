#ifndef INTERFACES_ACTUATOR_IF_H
#define INTERFACES_ACTUATOR_IF_H

#include "interface_types.h"

typedef struct
{
  void *ctx;
  void (*apply)(void *ctx, const actuator_output_t *out);
} actuator_if_t;

#endif
