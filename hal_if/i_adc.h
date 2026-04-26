#ifndef HAL_IF_I_ADC_H
#define HAL_IF_I_ADC_H

#include <stdint.h>
#include "i_types.h"

typedef struct
{
  void *ctx;
  uint16_t (*read_raw)(void *ctx, sensor_id_t id);
} i_adc_t;

#endif
