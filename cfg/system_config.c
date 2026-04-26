#include "system_config.h"
#include "../domain/dmx_strategy.h"

static uint16_t clamp_u16(uint16_t v, uint16_t min_v, uint16_t max_v)
{
  if(v < min_v)
  {
    return min_v;
  }
  if(v > max_v)
  {
    return max_v;
  }
  return v;
}

void cfg_get_default_params(system_params_t *params)
{
  if(params == 0)
  {
    return;
  }
  params->dmx_address = 1U;
  params->dmx_mode = DMX_MODE_2CH;
  params->igniter_delay_ms = 0U;
  params->oil_lock_delay_ms = 0U;
  params->tilt_protect_enable = true;
  params->language = 1; /* SL_LANG_CN */
}

void cfg_sanitize_params(system_params_t *params)
{
  uint16_t max_addr;
  if(params == 0)
  {
    return;
  }

  if((params->dmx_mode != DMX_MODE_2CH) && (params->dmx_mode != DMX_MODE_6CH))
  {
    params->dmx_mode = DMX_MODE_2CH;
  }

  max_addr = dmx_strategy_get_max_start_address(params->dmx_mode);
  if(max_addr > CFG_DMX_ADDR_MAX)
  {
    max_addr = CFG_DMX_ADDR_MAX;
  }

  params->dmx_address = clamp_u16(params->dmx_address, CFG_DMX_ADDR_MIN, max_addr);
  params->igniter_delay_ms = clamp_u16(params->igniter_delay_ms, CFG_DELAY_MIN_MS, CFG_DELAY_MAX_MS);
  params->oil_lock_delay_ms = clamp_u16(params->oil_lock_delay_ms, CFG_DELAY_MIN_MS, CFG_DELAY_MAX_MS);
}

bool cfg_voltage_raw_in_range(uint16_t raw)
{
#if (CFG_ENABLE_VOLTAGE_PROTECT == 0U)
  (void)raw;
  return true;
#else
  if(raw < CFG_VOLTAGE_LOW_RAW)
  {
    return false;
  }
  if(raw > CFG_VOLTAGE_HIGH_RAW)
  {
    return false;
  }
  return true;
#endif
}

bool cfg_pressure_sensor_fault(uint16_t raw)
{
  return (raw < CFG_PRESSURE_ADC_MIN_RAW);
}

uint8_t cfg_pressure_raw_to_percent(uint16_t raw)
{
  uint32_t num;
  uint32_t den;
  uint32_t pct;

  if(raw <= CFG_PRESSURE_ADC_MIN_RAW)
  {
    return 0U;
  }
  if(raw >= CFG_PRESSURE_ADC_MAX_RAW)
  {
    return 100U;
  }

  num = (uint32_t)(raw - CFG_PRESSURE_ADC_MIN_RAW) * 100U;
  den = (uint32_t)(CFG_PRESSURE_ADC_MAX_RAW - CFG_PRESSURE_ADC_MIN_RAW);
  pct = num / den;
  if(pct > 100U)
  {
    pct = 100U;
  }
  return (uint8_t)pct;
}
