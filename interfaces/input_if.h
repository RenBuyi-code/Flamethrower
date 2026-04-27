#ifndef INTERFACES_INPUT_IF_H
#define INTERFACES_INPUT_IF_H

#include <stdbool.h>
#include "interface_types.h"

typedef struct
{
  void *ctx;
  bool (*read)(void *ctx, input_id_t id);
} input_if_t;

#endif
