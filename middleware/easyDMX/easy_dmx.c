/**
 * @file    easy_dmx.c
 * @brief   easyDMX 轻量级 DMX512 协议解析器 — 实现
 *
 * ## 解析状态机
 *   easyDMX 使用增量解析状态机处理字节流：
 *
 *   [等待 Break]
 *       ↓ (收到 Break 事件)
 *   [已收到 Start Code?]
 *       ↓ 否 (第一个数据字节作为 Start Code)
 *   [解析通道数据 1~512]
 *       ↓ (再次收到 Break)
 *   [完成帧] → 写入 latest → 返回 [等待 Break]
 *
 * ## 两级 FIFO 架构
 *   本模块维护自己的字节 FIFO，与下层 BSP 的事件 FIFO 是两个独立缓冲区：
 *
 *   bsp_uart.c 的 FIFO：存储 bsp_uart_dmx_event_t { byte, is_break }
 *         ↓ (pop_event)
 *   easy_dmx.c 的 FIFO：存储 edmx_event_t { byte, flags }（扁平字节流）
 *         ↓ (edmx_rx_process)
 *   解析状态机：逐步构建 edmx_frame_t
 *
 * ## 帧有效性判断
 *   1. 必须有 Start Code（收到 Break 后的第一个字节）
 *   2. Start Code 必须为 0x00（标准 DMX512）
 *   3. slot_count 必须 > 0（有通道数据）
 *
 * @note    帧解析在 dmx_task 上下文中串行执行，无需加锁
 */

#include "easy_dmx.h"
#include <string.h>

/**
 * @brief   判断一个整数是否为 2 的幂次方
 *
 * @param[in] value  待检测值
 * @return    是否为 2 的幂次方
 *
 * @note    利用二进制特性：2^n 只有最高位为 1，如 8=0b1000
 *          2^n - 1 的二进制为连续 n 个 1，如 7=0b0111
 *          2^n & (2^n - 1) = 0（仅当 2^n 时成立）
 */
static bool is_power_of_two(size_t value)
{
  return (value != 0U) && ((value & (value - 1U)) == 0U);
}

/**
 * @brief   向字节 FIFO 写入一个字节（不检查边界）
 *
 * @param[in,out] fifo  FIFO 实例
 * @param[in]     value 要写入的字节
 *
 * @note    调用者需确保 FIFO 未满（fifo_free() >= 1）
 */
static void fifo_write_byte(edmx_fifo_t *fifo, uint8_t value)
{
  fifo->buffer[fifo->head & fifo->mask] = value;
  fifo->head++;
}

/**
 * @brief   从字节 FIFO 读取一个字节（不检查边界）
 *
 * @param[in,out] fifo  FIFO 实例
 * @return    读取到的字节
 *
 * @note    调用者需确保 FIFO 非空（fifo_used() >= 1）
 */
static uint8_t fifo_read_byte(edmx_fifo_t *fifo)
{
  uint8_t value;
  value = fifo->buffer[fifo->tail & fifo->mask];
  fifo->tail++;
  return value;
}

/**
 * @brief   重置解析状态机（收到 Break 后调用）
 *
 * 收到 Break 信号时，旧的解析状态必须丢弃，重新开始。
 *
 * @param[in,out] rx  接收器实例
 */
static void parser_reset(edmx_rx_t *rx)
{
  rx->parser_slot_count = 0U;
  rx->parser_start_code = 0U;
  rx->parser_has_start_code = false;
  rx->parser_overflow = false;
}

/**
 * @brief   完成当前帧的解析（收到 Break 后或超时）
 *
 * 将增量构建中的 parser_channels 拷贝到 latest 帧快照。
 * 只有符合以下全部条件的帧才会被接受：
 *   1. 已收到 Start Code
 *   2. Start Code == 0x00（标准 DMX）
 *   3. 通道数 > 0
 *
 * @param[in,out] rx      接收器实例
 * @param[in]     now_ms  当前系统时间（毫秒）
 */
static void finalize_frame(edmx_rx_t *rx, uint32_t now_ms)
{
  /* 尚未收到 Start Code，该帧无效 */
  if(rx->parser_has_start_code == false)
  {
    return;
  }

  /* Start Code 非 0（可能是其他协议如 RDM，不处理）*/
  if(rx->parser_start_code != 0U)
  {
    rx->stats.frames_nonzero_start++;
    return;
  }

  /* 无通道数据的短帧 */
  if(rx->parser_slot_count == 0U)
  {
    rx->stats.frames_short++;
    return;
  }

  /* 通道数超过 512 */
  if(rx->parser_overflow)
  {
    rx->stats.frames_long++;
  }

  /* 写入帧快照 */
  rx->latest.start_code = rx->parser_start_code;
  rx->latest.slot_count = (uint16_t)rx->parser_slot_count;
  memcpy(rx->latest.channels, rx->parser_channels, sizeof(rx->parser_channels));
  rx->latest.sequence++;
  rx->latest.tick_ms = now_ms;
  rx->latest.valid = true;

  rx->last_frame_tick_ms = now_ms;
  rx->stats.frames_ok++;
}

/**
 * @brief   从内部 FIFO 中弹出一个事件（edmx_event_t）
 *
 * @param[in,out] rx   接收器实例
 * @param[out]    evt  读取到的事件
 * @return         是否成功读取
 *
 * @note    每次读取 2 字节：flags 和 byte
 */
static bool pop_event(edmx_rx_t *rx, edmx_event_t *evt)
{
  if((rx == 0) || (evt == 0))
  {
    return false;
  }

  /* FIFO 中不足一个事件（每个事件占 2 字节） */
  if(edmx_fifo_used(&rx->fifo) < sizeof(edmx_event_t))
  {
    return false;
  }

  evt->flags = fifo_read_byte(&rx->fifo);
  evt->byte = fifo_read_byte(&rx->fifo);
  return true;
}

bool edmx_fifo_init(edmx_fifo_t *fifo, uint8_t *storage, size_t size_bytes)
{
  /* 参数合法性检查：非空、2 的幂次方、最小 2 字节 */
  if((fifo == 0) || (storage == 0) || (size_bytes < 2U) || (is_power_of_two(size_bytes) == false))
  {
    return false;
  }

  fifo->head = 0U;
  fifo->tail = 0U;
  fifo->mask = (uint32_t)(size_bytes - 1U);   /* 例：1024 → 0x3FF，用于位与取模 */
  fifo->buffer = storage;
  fifo->overruns = 0U;
  memset(storage, 0, size_bytes);
  return true;
}

size_t edmx_fifo_used(const edmx_fifo_t *fifo)
{
  if(fifo == 0)
  {
    return 0U;
  }
  /* head - tail 自动处理绕回（如 1023 - 0 = 1023，0 - 1023 = 1）*/
  return (size_t)(fifo->head - fifo->tail);
}

size_t edmx_fifo_free(const edmx_fifo_t *fifo)
{
  if(fifo == 0)
  {
    return 0U;
  }
  return (size_t)(fifo->mask + 1U) - edmx_fifo_used(fifo);
}

bool edmx_rx_init(edmx_rx_t *rx, uint8_t *fifo_storage, size_t fifo_size_bytes, uint32_t online_timeout_ms)
{
  if((rx == 0) || (fifo_storage == 0))
  {
    return false;
  }

  memset(rx, 0, sizeof(*rx));
  if(edmx_fifo_init(&rx->fifo, fifo_storage, fifo_size_bytes) == false)
  {
    return false;
  }

  rx->online_timeout_ms = online_timeout_ms;
  parser_reset(rx);
  return true;
}

bool edmx_rx_push_event(edmx_rx_t *rx, const edmx_event_t *evt)
{
  if((rx == 0) || (evt == 0))
  {
    return false;
  }

  /* FIFO 空间不足，丢弃事件并计数 */
  if(edmx_fifo_free(&rx->fifo) < sizeof(edmx_event_t))
  {
    rx->fifo.overruns++;
    rx->stats.fifo_overruns = rx->fifo.overruns;
    rx->stats.bytes_dropped += (uint32_t)sizeof(edmx_event_t);
    return false;
  }

  /* 先写 flags，后写 byte（与小端存储对齐）*/
  fifo_write_byte(&rx->fifo, evt->flags);
  fifo_write_byte(&rx->fifo, evt->byte);
  return true;
}

/**
 * @brief   解析 FIFO 中的全部事件，构建 DMX 帧
 *
 * 增量状态机逻辑：
 *   1. 收到 Break 事件：
 *      - 先 finalize_frame() 完成上一帧（如果有）
 *      - 再 parser_reset() 开始新一帧
 *   2. 收到普通字节（且尚未有 Start Code）：
 *      - 第一个字节作为 Start Code 记录
 *   3. 收到普通字节（已有 Start Code）：
 *      - 存入 parser_channels[]
 *      - 超过 512 时标记 overflow
 *
 * @param[in,out] rx      接收器实例
 * @param[in]     now_ms  当前系统时间（毫秒）
 */
void edmx_rx_process(edmx_rx_t *rx, uint32_t now_ms)
{
  edmx_event_t evt;

  if(rx == 0)
  {
    return;
  }

  /* 循环处理 FIFO 中所有事件 */
  while(pop_event(rx, &evt))
  {
    /* ---- 分支一：Break 信号 = 新帧开始 ---- */
    if((evt.flags & EDMX_EVENT_FLAG_BREAK) != 0U)
    {
      rx->stats.breaks_seen++;
      finalize_frame(rx, now_ms);   /* 完成上一帧 */
      parser_reset(rx);             /* 重置状态机，开始新帧 */
      continue;
    }

    /* ---- 分支二：Start Code（Break 后的第一个字节） ---- */
    if(rx->parser_has_start_code == false)
    {
      rx->parser_has_start_code = true;
      rx->parser_start_code = evt.byte;
      continue;
    }

    /* ---- 分支三：通道数据 ---- */
    if(rx->parser_slot_count < EDMX_UNIVERSE_SIZE)
    {
      /* 只有标准 DMX（Start Code=0）才记录通道数据 */
      if(rx->parser_start_code == 0U)
      {
        rx->parser_channels[rx->parser_slot_count] = evt.byte;
      }
      rx->parser_slot_count++;
    }
    else
    {
      /* 超过 512 通道，标记溢出 */
      rx->parser_overflow = true;
    }
  }
}

bool edmx_rx_copy_latest(const edmx_rx_t *rx, edmx_frame_t *out)
{
  if((rx == 0) || (out == 0) || (rx->latest.valid == false))
  {
    return false;
  }

  *out = rx->latest;
  return true;
}

/**
 * @brief   查询 DMX 信号是否在线
 *
 * 通过判断"最近一帧距今是否超过超时阈值"来判定。
 *
 * @param[in] rx      接收器实例
 * @param[in] now_ms  当前系统时间（毫秒）
 * @return    是否在线
 *
 * @note    now_ms 使用无符号减法，自动处理回绕（如 100 - 0xFFFFFFF0）
 */
bool edmx_rx_is_online(const edmx_rx_t *rx, uint32_t now_ms)
{
  if((rx == 0) || (rx->latest.valid == false))
  {
    return false;
  }

  if((now_ms - rx->last_frame_tick_ms) > rx->online_timeout_ms)
  {
    return false;
  }

  return true;
}

const edmx_stats_t *edmx_rx_get_stats(const edmx_rx_t *rx)
{
  if(rx == 0)
  {
    return 0;
  }
  return &rx->stats;
}
