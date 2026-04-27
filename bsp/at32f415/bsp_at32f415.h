#ifndef BSP_AT32F415_BSP_AT32F415_H
#define BSP_AT32F415_BSP_AT32F415_H

#include <stdbool.h>
#include "../../interfaces/adc_if.h"
#include "../../interfaces/actuator_if.h"
#include "../../interfaces/dmx_rx_if.h"
#include "../../interfaces/input_if.h"
#include "../../interfaces/storage_if.h"

typedef struct
{
  adc_if_t adc;
  input_if_t input;
  actuator_if_t actuator;
  dmx_rx_if_t dmx;
  storage_if_t storage;
} bsp_hal_bundle_t;

void bsp_at32f415_bind(bsp_hal_bundle_t *bundle);
bool bsp_at32f415_is_user_mode(void);

#endif
