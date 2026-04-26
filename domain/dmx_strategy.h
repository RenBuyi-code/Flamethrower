#ifndef DOMAIN_DMX_STRATEGY_H
#define DOMAIN_DMX_STRATEGY_H

#include <stdbool.h>
#include <stdint.h>
#include "../hal_if/i_types.h"

bool dmx_strategy_build_intent(
  dmx_mode_t mode,
  const uint8_t *channels,
  uint16_t start_addr,
  dmx_intent_t *intent);
uint16_t dmx_strategy_get_max_start_address(dmx_mode_t mode);
bool dmx_strategy_is_valid_start_address(dmx_mode_t mode, uint16_t start_addr);

#endif
