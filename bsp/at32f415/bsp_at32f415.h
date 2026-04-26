#ifndef BSP_AT32F415_BSP_AT32F415_H
#define BSP_AT32F415_BSP_AT32F415_H

#include <stdbool.h>
#include "../../hal_if/i_adc.h"
#include "../../hal_if/i_actuator.h"
#include "../../hal_if/i_dmx.h"
#include "../../hal_if/i_input.h"
#include "../../hal_if/i_storage.h"

typedef struct
{
  i_adc_t adc;
  i_input_t input;
  i_actuator_t actuator;
  i_dmx_t dmx;
  i_storage_t storage;
} bsp_hal_bundle_t;

void bsp_at32f415_bind(bsp_hal_bundle_t *bundle);
bool bsp_at32f415_is_user_mode(void);

#endif
