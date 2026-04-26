#ifndef HAL_IF_I_STORAGE_H
#define HAL_IF_I_STORAGE_H

#include <stdbool.h>
#include "i_types.h"

typedef struct
{
  void *ctx;
  bool (*load_params)(void *ctx, system_params_t *out);
  bool (*save_params)(void *ctx, const system_params_t *in);
} i_storage_t;

#endif
