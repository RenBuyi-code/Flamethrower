#ifndef INTERFACES_STORAGE_IF_H
#define INTERFACES_STORAGE_IF_H

#include <stdbool.h>
#include "interface_types.h"

typedef struct
{
  void *ctx;
  bool (*load_params)(void *ctx, system_params_t *out);
  bool (*save_params)(void *ctx, const system_params_t *in);
} storage_if_t;

#endif
