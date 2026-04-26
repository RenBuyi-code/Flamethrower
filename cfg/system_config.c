/**
 * @file    system_config.c
 * @brief   系统配置实现
 *
 * 系统配置模块，负责：
 *   - 提供系统默认参数
 *   - 校验和修正系统参数
 *   - 电压和压力传感器范围检查
 *   - 压力值转换为百分比
 *
 * 设计思路：
 *   - 集中管理所有配置参数
 *   - 确保参数在合理范围内
 *   - 提供传感器状态检查函数
 *   - 与其他模块的关系：
 *     - domain/dmx_strategy：用于计算DMX最大地址
 *     - app_core：使用参数配置系统
 */

#include "system_config.h"
#include "../domain/dmx_strategy.h"

/**
 * @brief   限制无符号16位数值在范围内
 *
 * @param[in] v      输入值
 * @param[in] min_v  最小值
 * @param[in] max_v  最大值
 * @return    限制后的值
 */
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

/**
 * @brief   获取默认系统参数
 *
 * @param[out] params  系统参数结构体指针
 *
 * 默认参数：
 *   - DMX地址：1
 *   - DMX模式：2CH
 *   - 点火延迟：0ms
 *   - 油路锁定延迟：0ms
 *   - 倾斜保护：启用
 *   - 语言：中文
 */
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

/**
 * @brief   校验和修正系统参数
 *
 * @param[inout] params  系统参数结构体指针
 *
 * 校验和修正内容：
 *   - DMX模式：确保为有效值（2CH或6CH）
 *   - DMX地址：根据模式限制在有效范围内
 *   - 点火延迟：限制在允许范围内
 *   - 油路锁定延迟：限制在允许范围内
 *
 * @note    此函数会修改传入的参数，确保所有值都在有效范围内
 */
void cfg_sanitize_params(system_params_t *params)
{
  uint16_t max_addr;
  if(params == 0)
  {
    return;
  }

  /** 校验DMX模式 */
  if((params->dmx_mode != DMX_MODE_2CH) && (params->dmx_mode != DMX_MODE_6CH))
  {
    params->dmx_mode = DMX_MODE_2CH;
  }

  /** 计算最大DMX地址 */
  max_addr = dmx_strategy_get_max_start_address(params->dmx_mode);
  if(max_addr > CFG_DMX_ADDR_MAX)
  {
    max_addr = CFG_DMX_ADDR_MAX;
  }

  /** 限制各参数在有效范围内 */
  params->dmx_address = clamp_u16(params->dmx_address, CFG_DMX_ADDR_MIN, max_addr);
  params->igniter_delay_ms = clamp_u16(params->igniter_delay_ms, CFG_DELAY_MIN_MS, CFG_DELAY_MAX_MS);
  params->oil_lock_delay_ms = clamp_u16(params->oil_lock_delay_ms, CFG_DELAY_MIN_MS, CFG_DELAY_MAX_MS);
}

/**
 * @brief   检查电压是否在正常范围内
 *
 * @param[in] raw  电压传感器原始值
 * @return    是否在正常范围内
 *
 * 检查逻辑：
 *   - 如果CFG_ENABLE_VOLTAGE_PROTECT为0，始终返回true
 *   - 否则检查电压是否低于最低值或高于最高值
 */
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

/**
 * @brief   检查压力传感器是否故障
 *
 * @param[in] raw  压力传感器原始值
 * @return    是否故障
 *
 * 判断条件：原始值小于最小阈值
 */
bool cfg_pressure_sensor_fault(uint16_t raw)
{
  return (raw < CFG_PRESSURE_ADC_MIN_RAW);
}

/**
 * @brief   将压力传感器原始值转换为百分比
 *
 * @param[in] raw  压力传感器原始值
 * @return    压力百分比（0-100）
 *
 * 转换算法：
 *   1. 如果原始值小于最小阈值，返回0%
 *   2. 如果原始值大于最大阈值，返回100%
 *   3. 否则计算相对于范围的百分比
 *
 * 公式：percent = (raw - MIN) / (MAX - MIN) * 100
 */
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
