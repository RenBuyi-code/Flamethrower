/**
 * @file    dmx_strategy.h
 * @brief   DMX512 控制策略 — 领域层接口定义
 *
 * ## 职责
 *   本模块是 DMX512 领域模型的核心，负责：
 *   - 将原始 DMX512 通道数据转换为机器执行意向（intent）
 *   - 验证 DMX 起始地址的合法性
 *
 * ## 为什么需要领域层
 *   DMX512 通道值（如 0xFF）是原始的、未经解释的。
 *   领域层负责解释这些值的语义，生成业务意图：
 *   - "通道1 >= 254？" → "请求喷射"
 *   - "通道2 in [0~49] or [201~255]？" → "请求泄压"
 *
 * ## 支持的 DMX 工作模式
 *   - DMX_MODE_2CH：2 通道模式（fire + mode），适合简单控制
 *   - DMX_MODE_6CH：6 通道模式（fire + time + mode 等），适合精细控制
 *
 * ## 通道映射（相对起始地址）
 *   2CH 模式：
 *     基址 + 0 → fire_ch    （火力通道）
 *     基址 + 1 → mode_ch    （模式通道：0~49=泄压，201~255=泄压，50~200=正常）
 *
 *   6CH 模式：
 *     基址 + 0 → （保留）
 *     基址 + 1 → （保留）
 *     基址 + 2 → fire_ch    （火力通道）
 *     基址 + 3 → time_ch    （喷射时长：0=无限，1~254=时长×10ms，255=无限）
 *     基址 + 4 → （保留）
 *     基址 + 5 → mode_ch    （模式通道，同 2CH）
 *
 * ## 术语
 *   - start_addr：DMX 起始地址（1~512），用户可配置
 *   - base：计算后的数组索引（start_addr - 1）
 *   - intent：执行意向结构，包含 request_fire、request_relief、fire_duration_ms
 */

#ifndef APP_RULES_DMX_STRATEGY_H
#define APP_RULES_DMX_STRATEGY_H

#include <stdbool.h>
#include <stdint.h>
#include "../../interfaces/interface_types.h"

/**
 * @brief   将 DMX 通道数据转换为执行意向
 *
 * @param[in]     mode       DMX 工作模式（2CH 或 6CH）
 * @param[in]     channels   指向 DMX 帧通道数组的指针（固定 512 字节）
 * @param[in]     start_addr DMX 起始地址（1~512，用户配置）
 * @param[out]    intent     输出的执行意向结构
 * @return         转换是否成功（参数合法且帧有效）
 *
 * ## 核心逻辑
 *   1. 验证 start_addr 是否在合法范围内
 *   2. 读取 mode_ch，判断是否为泄压请求
 *   3. 若非泄压，读取 fire_ch，判断是否请求喷射
 *   4. 6CH 模式下额外读取 time_ch 计算喷射时长
 *
 * ## 喷射触发条件
 *   - fire_ch >= 254（DMX 值 254~255 均视为触发）
 *   - 注意：fire_ch == 255 也算触发，但规范建议使用 254 作为触发阈值
 */
bool dmx_strategy_build_intent(
  dmx_mode_t mode,
  const uint8_t *channels,
  uint16_t start_addr,
  dmx_intent_t *intent);

/**
 * @brief   获取指定 DMX 模式下的最大起始地址
 *
 * @param[in] mode  DMX 工作模式
 * @return    最大允许的起始地址
 *
 * @note    2CH 模式最大 511（留 1 通道给 mode_ch）
 *          6CH 模式最大 507（留 5 通道给其他参数）
 */
uint16_t dmx_strategy_get_max_start_address(dmx_mode_t mode);

/**
 * @brief   验证起始地址是否合法
 *
 * @param[in] mode        DMX 工作模式
 * @param[in] start_addr  待验证的起始地址
 * @return    地址是否合法
 *
 * @note    起始地址从 1 开始（DMX512 标准规定），不能为 0
 */
bool dmx_strategy_is_valid_start_address(dmx_mode_t mode, uint16_t start_addr);

#endif
