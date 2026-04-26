#include "dmx_strategy.h"

static bool is_relief_window(uint8_t value)
{
  if(value <= 49U)
  {
    return true;
  }
  if(value >= 201U)
  {
    return true;
  }
  return false;
}

uint16_t dmx_strategy_get_max_start_address(dmx_mode_t mode)
{
  if(mode == DMX_MODE_6CH)
  {
    return 507U;
  }
  if(mode == DMX_MODE_2CH)
  {
    return 511U;
  }
  return 1U;
}

bool dmx_strategy_is_valid_start_address(dmx_mode_t mode, uint16_t start_addr)
{
  if(start_addr < 1U)
  {
    return false;
  }
  return (start_addr <= dmx_strategy_get_max_start_address(mode));
}

bool dmx_strategy_build_intent(
  dmx_mode_t mode,
  const uint8_t *channels,
  uint16_t start_addr,
  dmx_intent_t *intent)
{
  uint16_t base;
  uint8_t fire_ch;
  uint8_t mode_ch;
  uint8_t time_ch;

  if((channels == 0) || (intent == 0) || (dmx_strategy_is_valid_start_address(mode, start_addr) == false))
  {
    return false;
  }

  intent->request_fire = false;
  intent->request_relief = false;
  intent->fire_duration_ms = 0U;

  base = (uint16_t)(start_addr - 1U);

  if(mode == DMX_MODE_2CH)
  {
    fire_ch = channels[base];
    mode_ch = channels[(uint16_t)(base + 1U)];

    if(is_relief_window(mode_ch))
    {
      intent->request_relief = true;
      intent->request_fire = false;
      return true;
    }

    intent->request_fire = (fire_ch >= 254U);
    intent->fire_duration_ms = 0U;
    return true;
  }

  if(mode == DMX_MODE_6CH)
  {
    fire_ch = channels[(uint16_t)(base + 2U)];
    time_ch = channels[(uint16_t)(base + 3U)];
    mode_ch = channels[(uint16_t)(base + 5U)];

    if(is_relief_window(mode_ch))
    {
      intent->request_relief = true;
      intent->request_fire = false;
      return true;
    }

    intent->request_fire = (fire_ch >= 254U);

    if((time_ch == 0U) || (time_ch == 255U))
    {
      intent->fire_duration_ms = 0U;
    }
    else
    {
      intent->fire_duration_ms = (uint16_t)time_ch * 10U;
    }
    return true;
  }

  return false;
}
