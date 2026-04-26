/**
 * @file    easy_dmx.h
 * @brief   easyDMX — 轻量级 DMX512 协议解析中间件
 *
 * ## 概述
 *   easyDMX 是一套用于微控制器平台的 DMX512 接收协议栈。
 *   它从下层 BSP 驱动（轮询或中断驱动均可）获取字节事件，
 *   解析出完整的 DMX 帧，并提供最新帧数据的快照拷贝。
 *
 * ## DMX512 协议概要
 *   DMX512 是一种单向异步串行协议（250kbps，8N2）：
 *   1. Break 信号（低电平 ≥ 88µs）— 标记一帧开始
 *   2. Mark After Break (MAB)：高电平 ≥ 8µs
 *   3. Start Code（1 字节）：通常为 0x00（标准 DMX）
 *   4. 通道 1~512（每通道 1 字节）：每帧最多 512 通道
 *
 * ## 架构分层
 *   [上层应用] ← edmx_rx_t ← edmx_rx_process() ← edmx_rx_push_event()
 *   [本模块]      easyDMX 协议解析器
 *   [下层 BSP]    bsp_uart_poll_event() ← hal.dmx.poll_byte()
 *   [硬件]        USART1 中断 + FIFO
 *
 * ## 线程安全说明
 *   - edmx_rx_push_event()：可从中断或任务调用（非原子，需外部同步）
 *   - edmx_rx_process()：应在单个任务上下文中调用
 *   - edmx_rx_copy_latest()：读取时需确保 process 已完成或禁止调度
 *
 * ## 使用示例
 *   @code
 *   // 初始化
 *   edmx_rx_t dmx_rx;
 *   uint8_t fifo_buf[1024];
 *   edmx_rx_init(&dmx_rx, fifo_buf, sizeof(fifo_buf), 3000U);
 *
 *   // 在 DMX 任务中：喂入事件
 *   bsp_uart_dmx_event_t evt;
 *   while (bsp_uart_dmx_poll_event(&evt)) {
 *       edmx_event_t e = { evt.byte, evt.is_break ? EDMX_EVENT_FLAG_BREAK : 0 };
 *       edmx_rx_push_event(&dmx_rx, &e);
 *   }
 *
 *   // 处理（解析）事件
 *   edmx_rx_process(&dmx_rx, xTaskGetTickCount());
 *
 *   // 读取最新帧
 *   edmx_frame_t frame;
 *   if (edmx_rx_copy_latest(&dmx_rx, &frame)) {
 *       uint8_t ch1 = frame.channels[0]; // 通道 1 的值
 *   }
 *   @endcode
 */

#ifndef MIDDLEWARE_EASY_DMX_H
#define MIDDLEWARE_EASY_DMX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief DMX512 标准 universe 尺寸（最大通道数） */
#define EDMX_UNIVERSE_SIZE                512U

/** @brief Break 事件标志（写入 edmx_event_t.flags） */
#define EDMX_EVENT_FLAG_BREAK             0x01U

/**
 * @brief   DMX 事件结构
 *
 * 表示从下层 BSP 获取的单个字节事件。
 * flags 中的 EDMX_EVENT_FLAG_BREAK 位标识是否为 Break 信号。
 */
typedef struct
{
  /** @brief 字节数据（0x00~0xFF） */
  uint8_t byte;
  /** @brief 事件标志，目前仅 bit0 有意义：1=Break 信号，0=普通数据字节 */
  uint8_t flags;
} edmx_event_t;

/**
 * @brief   字节流环形 FIFO（无锁单生产者单消费者）
 *
 * 本模块内部使用的字节 FIFO，用于在事件推送和解析之间缓冲。
 * 采用 head/tail 双指针，无锁设计（假设单一生产者/消费者）。
 *
 * @note    与 bsp_uart.c 中的事件 FIFO 是两个不同概念：
 *          - bsp_uart.c 的 FIFO：存储 bsp_uart_dmx_event_t（字节 + is_break）
 *          - 本模块的 FIFO：存储 edmx_event_t（字节 + flags）
 */
typedef struct
{
  /** @brief 下一条写入位置（写入者更新） */
  uint32_t head;
  /** @brief 下一条读取位置（读取者更新） */
  uint32_t tail;
  /** @brief 缓冲区大小掩码（size - 1，用于位与取模） */
  uint32_t mask;
  /** @brief 字节存储缓冲区指针（由调用者分配） */
  uint8_t *buffer;
  /** @brief FIFO 溢出计数（写入时发现已满则 +1） */
  uint32_t overruns;
} edmx_fifo_t;

/**
 * @brief   解析完成的 DMX 帧快照
 *
 * 当一帧 DMX 数据完整解析后，结果保存在此结构中。
 * 通过 edmx_rx_copy_latest() 获取快照拷贝。
 */
typedef struct
{
  /** @brief Start Code（通常为 0x00） */
  uint8_t start_code;
  /** @brief 本帧实际通道数（1~512） */
  uint16_t slot_count;
  /** @brief 通道数据数组（固定 512 字节，未使用的尾部为 0） */
  uint8_t channels[EDMX_UNIVERSE_SIZE];
  /** @brief 帧序号（每成功解析一帧 +1，溢出后回绕） */
  uint32_t sequence;
  /** @brief 本帧解析完成时的时间戳（毫秒） */
  uint32_t tick_ms;
  /** @brief 本帧是否有效（ture=有效，false=尚无有效帧） */
  bool valid;
} edmx_frame_t;

/**
 * @brief   DMX 接收统计信息
 *
 * 用于诊断 DMX 信号质量和协议解析状态。
 */
typedef struct
{
  /** @brief 已检测到的 Break 信号总数 */
  uint32_t breaks_seen;
  /** @brief 成功解析的标准 DMX 帧数（Start Code = 0） */
  uint32_t frames_ok;
  /** @brief 短帧数（收到 Break 但未收到任何通道数据） */
  uint32_t frames_short;
  /** @brief 长帧数（通道数据超过 512，上溢） */
  uint32_t frames_long;
  /** @brief 非标准帧数（Start Code ≠ 0，不是我方 DMX 信号） */
  uint32_t frames_nonzero_start;
  /** @brief 事件 FIFO 溢出次数（下层喂入过快） */
  uint32_t fifo_overruns;
  /** @brief 丢弃的字节总数 */
  uint32_t bytes_dropped;
} edmx_stats_t;

/**
 * @brief   easyDMX 接收器实例
 *
 * 包含协议解析所需的全部状态：
 *   - 字节流 FIFO
 *   - 最新解析完成的帧
 *   - 当前正在解析的帧（增量构建中）
 *   - 统计计数
 *
 * @note    通常一个设备只需要一个实例
 */
typedef struct
{
  /** @brief 字节流 FIFO */
  edmx_fifo_t fifo;
  /** @brief 最新解析完成的帧（快照） */
  edmx_frame_t latest;
  /** @brief 统计计数 */
  edmx_stats_t stats;
  /** @brief 离线超时阈值（毫秒），超过此时间无新帧则判定为离线 */
  uint32_t online_timeout_ms;
  /** @brief 最近一次帧的时间戳（毫秒） */
  uint32_t last_frame_tick_ms;
  /** @brief 当前正在解析的帧的通道计数 */
  uint32_t parser_slot_count;
  /** @brief 当前帧的 Start Code */
  uint8_t parser_start_code;
  /** @brief 是否已接收到 Start Code */
  bool parser_has_start_code;
  /** @brief 当前帧是否发生上溢（超过 512 通道） */
  bool parser_overflow;
  /** @brief 解析中的帧的通道数据缓冲区 */
  uint8_t parser_channels[EDMX_UNIVERSE_SIZE];
} edmx_rx_t;

/* ================================================================ */
/*                        API 函数声明                                */
/* ================================================================ */

/**
 * @brief   初始化字节流 FIFO
 *
 * @param[in] fifo          FIFO 实例指针
 * @param[in] storage       调用者分配的缓冲区（必须是 2 的幂次方大小）
 * @param[in] size_bytes    缓冲区大小（必须为 2 的幂次方，最小 2）
 * @return    初始化是否成功
 */
bool edmx_fifo_init(edmx_fifo_t *fifo, uint8_t *storage, size_t size_bytes);

/**
 * @brief   获取 FIFO 中已使用字节数
 * @param[in] fifo  FIFO 实例指针
 * @return    已存储的字节数
 */
size_t edmx_fifo_used(const edmx_fifo_t *fifo);

/**
 * @brief   获取 FIFO 剩余可用字节数
 * @param[in] fifo  FIFO 实例指针
 * @return    可写入的字节数
 */
size_t edmx_fifo_free(const edmx_fifo_t *fifo);

/**
 * @brief   初始化 easyDMX 接收器
 *
 * @param[in] rx                 接收器实例指针
 * @param[in] fifo_storage        事件 FIFO 缓冲区（由调用者分配）
 * @param[in] fifo_size_bytes     FIFO 缓冲区大小（建议 ≥ 1024）
 * @param[in] online_timeout_ms   离线超时阈值（毫秒），通常 1000~3000ms
 * @return    初始化是否成功
 */
bool edmx_rx_init(edmx_rx_t *rx, uint8_t *fifo_storage, size_t fifo_size_bytes, uint32_t online_timeout_ms);

/**
 * @brief   向解析器推送一个 DMX 事件
 *
 * @param[in] rx   接收器实例指针
 * @param[in] evt  事件指针（byte + flags）
 * @return    推送是否成功；FIFO 满时返回 false
 *
 * @note    可从中断处理程序调用，需外部保证单一生产者
 */
bool edmx_rx_push_event(edmx_rx_t *rx, const edmx_event_t *evt);

/**
 * @brief   解析 FIFO 中的全部事件，构建 DMX 帧
 *
 * 消费 FIFO 中的所有事件，按 DMX 协议解析出完整帧。
 * 解析完成后更新 rx->latest。
 *
 * @param[in] rx      接收器实例指针
 * @param[in] now_ms  当前系统时间（毫秒），用于打时间戳
 *
 * @note    应在 dmx_task 中定期调用（建议每 1~10ms 调用一次）
 */
void edmx_rx_process(edmx_rx_t *rx, uint32_t now_ms);

/**
 * @brief   拷贝最新的有效 DMX 帧（快照拷贝）
 *
 * @param[in]  rx   接收器实例指针
 * @param[out] out  拷贝目标
 * @return    拷贝是否成功（无有效帧时返回 false）
 *
 * @note    这是安全拷贝，即使在解析进行中调用也不会被解析过程破坏
 */
bool edmx_rx_copy_latest(const edmx_rx_t *rx, edmx_frame_t *out);

/**
 * @brief   查询 DMX 信号是否在线
 *
 * @param[in] rx      接收器实例指针
 * @param[in] now_ms  当前系统时间（毫秒）
 * @return    是否在线（最近一帧在超时阈值内）
 *
 * @note    可用于检测 DMX 信号线是否断开或控制器是否关闭
 */
bool edmx_rx_is_online(const edmx_rx_t *rx, uint32_t now_ms);

/**
 * @brief   获取 DMX 接收统计信息指针
 * @param[in] rx  接收器实例指针
 * @return   统计结构体常量指针（不允许修改）
 */
const edmx_stats_t *edmx_rx_get_stats(const edmx_rx_t *rx);

#endif
