#ifndef CFG_SYSTEM_CONFIG_H
#define CFG_SYSTEM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "../hal_if/i_types.h"

#define CFG_DMX_ADDR_MIN                 1U
#define CFG_DMX_ADDR_MAX                 512U
#define CFG_DELAY_MIN_MS                 0U
#define CFG_DELAY_MAX_MS                 200U

/*
 * 压力传感器: 4-20mA, 采样电阻 "111" => 110R
 * ADC 12bit@3.3V:
 * 4mA  -> 0.44V -> ~546 counts
 * 19.5mA -> 2.145V -> ~2661 counts
 */
#define CFG_PRESSURE_ADC_MIN_RAW         546U
#define CFG_PRESSURE_ADC_MAX_RAW         2661U

/* 默认安全阈值(百分比) */
#define CFG_PRESSURE_READY_MIN_PCT       35U
#define CFG_PRESSURE_TARGET_PCT          100U
#define CFG_PRESSURE_REFILL_RESUME_PCT   97U
#define CFG_PRESSURE_FIRE_MIN_PCT        20U
#define CFG_PRESSURE_RELIEF_DONE_PCT     5U

#define CFG_PRESSURE_ERROR_TIMEOUT_MS    3000U
#define CFG_RELIEF_ERROR_TIMEOUT_MS      2000U
#define CFG_SELFTEST_PRESSURE_TIMEOUT_MS 13000U

#define CFG_DMX_LOST_TIMEOUT_MS          500U
#define CFG_VOLTAGE_ERROR_HOLD_MS        5000U

/*
 * 电压保护开关：
 * 0 = 调试阶段关闭 E3 电压保护（不影响其余功能联调）
 * 1 = 开启 E3 电压保护（按 10V/15V 对应阈值判定）
 */
#define CFG_ENABLE_VOLTAGE_PROTECT       0U

/* [留白待定] 下列电压原始阈值需根据硬件分压比和标定结果调整 */
#define CFG_VOLTAGE_LOW_RAW              1200U
#define CFG_VOLTAGE_HIGH_RAW             3200U

void cfg_get_default_params(system_params_t *params);
void cfg_sanitize_params(system_params_t *params);
bool cfg_voltage_raw_in_range(uint16_t raw);
uint8_t cfg_pressure_raw_to_percent(uint16_t raw);

#endif
