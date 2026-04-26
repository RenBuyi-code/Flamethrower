/**
 * @file    system_config.h
 * @brief   系统配置头文件
 *
 * 系统配置模块的头文件，定义了：
 *   - 系统参数结构体
 *   - 配置常量宏定义
 *   - 配置函数声明
 *
 * 配置常量包括：
 *   - DMX地址范围
 *   - 延迟时间范围
 *   - 压力传感器参数
 *   - 电压保护参数
 *   - 超时时间参数
 *
 * 设计思路：
 *   - 集中管理所有配置常量
 *   - 使用有意义的命名和注释
 *   - 便于后期调整和优化
 */

#ifndef CFG_SYSTEM_CONFIG_H
#define CFG_SYSTEM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "../hal_if/i_types.h"

/** @brief DMX地址最小值 */
#define CFG_DMX_ADDR_MIN                 1U
/** @brief DMX地址最大值 */
#define CFG_DMX_ADDR_MAX                 512U
/** @brief 延迟时间最小值（毫秒） */
#define CFG_DELAY_MIN_MS                 0U
/** @brief 延迟时间最大值（毫秒） */
#define CFG_DELAY_MAX_MS                 200U

/**
 * @brief   压力传感器参数
 *
 * 压力传感器型号：4-20mA电流输出
 * 采样电阻：111欧姆
 * ADC配置：12位分辨率，3.3V参考电压
 *
 * 电流-电压-ADC值对应关系：
 *   - 4mA  -> 0.44V -> ~546 counts
 *   - 19.5mA -> 2.145V -> ~2661 counts
 */
/** @brief 压力传感器ADC最小原始值（对应4mA） */
#define CFG_PRESSURE_ADC_MIN_RAW         546U
/** @brief 压力传感器ADC最大原始值（对应19.5mA） */
#define CFG_PRESSURE_ADC_MAX_RAW         2661U

/**
 * @brief   默认安全阈值（百分比）
 *
 * 压力百分比定义：
 *   - 就绪最小压力：35%
 *   - 目标压力：100%
 *   - 补压恢复压力：97%（压力低于此值时重新补压）
 *   - 点火最小压力：20%
 *   - 泄压完成压力：5%（压力低于此值时认为泄压完成）
 */
/** @brief 压力达到就绪状态的最小百分比 */
#define CFG_PRESSURE_READY_MIN_PCT       35U
/** @brief 压力目标百分比 */
#define CFG_PRESSURE_TARGET_PCT          100U
/** @brief 压力补压恢复百分比（低于此值开始补压） */
#define CFG_PRESSURE_REFILL_RESUME_PCT   97U
/** @brief 点火所需最小压力百分比 */
#define CFG_PRESSURE_FIRE_MIN_PCT        20U
/** @brief 泄压完成压力百分比（低于此值认为泄压完成） */
#define CFG_PRESSURE_RELIEF_DONE_PCT     5U

/** @brief 压力建立错误超时时间（毫秒） */
#define CFG_PRESSURE_ERROR_TIMEOUT_MS    13000U
/** @brief 泄压错误超时时间（毫秒） */
#define CFG_RELIEF_ERROR_TIMEOUT_MS      2000U
/** @brief 自检压力建立超时时间（毫秒） */
#define CFG_SELFTEST_PRESSURE_TIMEOUT_MS 13000U

/** @brief DMX信号丢失超时时间（毫秒） */
#define CFG_DMX_LOST_TIMEOUT_MS          500U
/** @brief 电压错误持续时间（毫秒，用于E3电压故障判定） */
#define CFG_VOLTAGE_ERROR_HOLD_MS        5000U

/**
 * @brief   电压保护开关
 *
 * 配置选项：
 *   - 0 = 调试阶段关闭E3电压保护（不影响其余功能联调）
 *   - 1 = 开启E3电压保护（按10V/15V对应阈值判定）
 */
/** @brief 电压保护功能开关（0=关闭, 1=开启） */
#define CFG_ENABLE_VOLTAGE_PROTECT       0U

/** @brief 电压原始阈值（需根据硬件分压比和标定结果调整） */
#define CFG_VOLTAGE_LOW_RAW              1200U
/** @brief 电压原始阈值上限 */
#define CFG_VOLTAGE_HIGH_RAW             3200U

/**
 * @brief   获取默认系统参数
 *
 * @param[out] params  系统参数结构体指针
 *
 * 获取默认参数值，用于初始化或参数损坏时恢复
 */
void cfg_get_default_params(system_params_t *params);

/**
 * @brief   校验和修正系统参数
 *
 * @param[inout] params  系统参数结构体指针
 *
 * 确保所有参数都在有效范围内
 */
void cfg_sanitize_params(system_params_t *params);

/**
 * @brief   检查电压是否在正常范围内
 *
 * @param[in] raw  电压传感器原始值
 * @return    是否在正常范围内
 */
bool cfg_voltage_raw_in_range(uint16_t raw);

/**
 * @brief   检查压力传感器是否故障
 *
 * @param[in] raw  压力传感器原始值
 * @return    是否故障
 */
bool cfg_pressure_sensor_fault(uint16_t raw);

/**
 * @brief   将压力传感器原始值转换为百分比
 *
 * @param[in] raw  压力传感器原始值
 * @return    压力百分比（0-100）
 */
uint8_t cfg_pressure_raw_to_percent(uint16_t raw);

#endif
