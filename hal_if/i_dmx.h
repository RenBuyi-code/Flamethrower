/**
 * @file    i_dmx.h
 * @brief   DMX512 接收 HAL 接口层定义
 *
 * 本文件定义了 DMX512 接收的硬件抽象接口，位于 BSP 层与上层协议栈之间。
 * 通过函数指针解耦硬件实现，支持轮询方式获取每个 DMX 字节及其 Break 状态。
 *
 * 数据流：
 *   硬件 USART 中断 → BSP 层软件 FIFO → 本接口 poll_byte() → easyDMX parser
 *
 * 使用方式：
 *   1. 硬件初始化（USART、DMA 等）由 BSP 层完成
 *   2. 上层调用 poll_byte() 轮询获取下一个 DMX 事件
 *   3. ctx 用于传递 BSP 层上下文（如 FIFO 句柄）
 */

#ifndef HAL_IF_I_DMX_H
#define HAL_IF_I_DMX_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   DMX 硬件抽象接口
 *
 * 通过本结构体，上层代码无需关心底层是中断驱动还是 DMA 驱动，
 * 只需调用 poll_byte() 即可获取 DMX 数据。
 *
 * @note    所有实现均通过中断上下文或临界区保护，确保线程安全
 */
typedef struct
{
  /** @brief 上下文指针，传递给 poll_byte 的 ctx 参数 */
  void *ctx;

  /**
   * @brief   从底层 FIFO 中轮询读取一个 DMX 事件
   *
   * @param[out] byte     读取到的字节数据
   * @param[out] is_break  指示该字节是否处于 Break 信号区间
   *                      - true:  Break 信号（通常为 0x00）
   *                      - false: 正常数据字节
   * @return   是否有数据可供读取
   *           - true:  数据已写入 byte 和 is_break
   *           - false: FIFO 为空，无数据
   */
  bool (*poll_byte)(void *ctx, uint8_t *byte, bool *is_break);
} i_dmx_t;

#endif
