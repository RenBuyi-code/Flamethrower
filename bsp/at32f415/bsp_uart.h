/**
 * @file    bsp_uart.h
 * @brief   AT32F415 USART1 DMX 接收驱动 — 板级支持包头文件
 *
 * 本模块实现 AT32F415 微控制器 USART1 的 DMX512 接收功能。
 *
 * ## 硬件连接
 *   DMX 信号线 → USART1_RX（通过电平转换器）
 *
 * ## DMX512 协议概要
 *   DMX512 每帧包含：
 *   1. Break 信号：低电平 ≥ 88µs（标记一帧开始）
 *   2. Mark After Break (MAB)：高电平 ≥ 8µs
 *   3. Start Code：1 字节（通常为 0x00）
 *   4. 512 通道数据：每通道 1 字节
 *
 * ## 实现方案
 *   采用【字节中断 + 软件 FIFO】方案：
 *   - USART1 每接收一个字节触发一次中断（RXNE）
 *   - USART1 帧错误（FERR）和 Break 中断（BFF）识别 Break 信号
 *   - 所有事件存入 256 字节软件 FIFO，供上层任务轮询读取
 *
 * ## 中断处理流程
 *   USART1_IRQHandler() → bsp_uart_dmx_irq_handler()
 *     - 检测到 FERR/BFF：标记 Break，压入 is_break=true 事件
 *     - 接收到正常字节：压入 is_break=false 事件
 *
 * @note    波特率固定为 250kbps，8N2（8数据位，无校验，2停止位）
 */

#ifndef BSP_AT32F415_BSP_UART_H
#define BSP_AT32F415_BSP_UART_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   DMX 接收事件结构
 *
 * 用于表示从 USART1 接收到的每一个字节事件。
 * is_break 标志该字节是否处于 Break 信号区间。
 */
typedef struct
{
  /** @brief 接收到的字节数据 */
  uint8_t byte;
  /** @brief Break 标志
   *   - true:  该字节是 Break 信号（通常为 0x00）
   *   - false: 正常数据字节
   */
  bool is_break;
} bsp_uart_dmx_event_t;

/**
 * @brief   初始化 DMX 串口接收
 *
 * 调用本函数后：
 *   - 软件 FIFO 被清空
 *   - USART1 中断被使能
 *   - 等待 DMX 信号输入
 *
 * @note    必须在系统时钟和 GPIO 初始化之后调用
 */
void bsp_uart_dmx_init(void);

/**
 * @brief   USART1 DMX 中断服务程序
 *
 * 由 USART1_IRQn 中断调用，负责：
 *   1. 检测 Break 信号：通过 FERR（帧错误）和 BFF（Break 检测）标志
 *   2. 读取数据字节：通过 RDBF（接收数据缓冲）标志
 *   3. 写入软件 FIFO：供 bsp_uart_dmx_poll_event() 读取
 *
 * @warning 本函数在中断上下文中执行，需避免耗时操作
 */
void bsp_uart_dmx_irq_handler(void);

/**
 * @brief   从软件 FIFO 中轮询读取一个 DMX 事件
 *
 * 供上层任务（如 dmx_task）调用，从 BSP 层 FIFO 中取出事件。
 *
 * @param[out] out  读取到的事件数据
 * @return         读取是否成功
 *                 - true:  成功读取一个事件
 *                 - false: FIFO 为空
 *
 * @note    该函数使用临界区保护（__disable_irq / __enable_irq），
 *          确保在中断与任务之间安全共享 FIFO
 */
bool bsp_uart_dmx_poll_event(bsp_uart_dmx_event_t *out);

#endif
