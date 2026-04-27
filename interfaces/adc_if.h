#ifndef INTERFACES_ADC_IF_H
#define INTERFACES_ADC_IF_H

#include <stdint.h>
#include "interface_types.h"

typedef struct
{
  void *ctx;
  uint16_t (*read_raw)(void *ctx, sensor_id_t id);
} adc_if_t;

#endif
