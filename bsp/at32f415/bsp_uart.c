/**
 * @file    bsp_uart.c
 * @brief   AT32F415 USART1 DMX 接收驱动 — 软件实现
 *
 * 本文件实现 AT32F415 微控制器 USART1 的 DMX512 接收驱动。
 *
 * ## 设计背景
 *   DMX512 协议规定 Break 信号期间线路保持低电平，
 *   此时若接收到的数据字节（0x00）不符合正常帧格式，
 *   会被标记为帧错误（FERR）。利用这一特性，
 *   我们通过检测 FERR 标志来识别 Break 信号的开始。
 *
 * ## 关键中断标志
 *   - FERR：帧错误标志，当接收到的字节停止位检测失败时置位
 *           DMX Break 期间的低电平会被误判为 0x00 的异常停止位，触发 FERR
 *   - BFF：Break 检测标志（部分 AT32 型号支持），专门检测 Break 信号
 *   - NERR：噪声错误标志
 *   - ROERR：溢出错误标志
 *   - RDBF：接收数据缓冲就绪，标识新字节已接收
 *
 * ## 软件 FIFO 设计
 *   使用 256 字节环形缓冲区，解耦中断与上层任务：
 *   - 中断处理程序（生产者）：将接收到的字节写入 FIFO
 *   - 上层任务（消费者）：从 FIFO 读取字节
 *   - 临界区保护：poll_event 使用 __disable_irq() 防止数据竞争
 *
 * ## Break 信号识别逻辑
 *   当 FERR 或 BFF 置位时：
 *     1. 清除所有错误标志
 *     2. 如果 RDBF 也置位，读取并丢弃该字节（Break 期间的假字节）
 *     3. 向 FIFO 压入 is_break=true 的特殊事件（数据为 0x00）
 *
 * ## 注意事项
 *   - 中断处理函数不进行任何耗时操作，仅写入 FIFO
 *   - overrun_count 记录 FIFO 溢出次数，可用于诊断
 */

#include "bsp_uart.h"
#include "../../project/inc/at32f415_conf.h"

/** @brief 软件 FIFO 深度，必须为 2 的幂次方 */
#define BSP_UART_DMX_FIFO_SIZE          256U
/** @brief 用于位与运算快速取模（2 的幂次方特性） */
#define BSP_UART_DMX_FIFO_MASK          (BSP_UART_DMX_FIFO_SIZE - 1U)

/**
 * @brief   DMX 接收软件 FIFO 结构（环形缓冲区）
 *
 * 采用首尾指针设计，无锁并发写入：
 *   - head：写入位置（中断写），递增到尾部时绕回
 *   - tail：读取位置（任务读），同上
 *   - 当 head == tail 时表示 FIFO 为空
 *   - 当 next_head == tail 时表示 FIFO 已满（溢出）
 */
typedef struct
{
  /** @brief 下一条写入位置（生产者指针） */
  volatile uint16_t head;
  /** @brief 下一条读取位置（消费者指针） */
  volatile uint16_t tail;
  /** @brief FIFO 溢出计数（当 FIFO 满时丢弃数据则递增） */
  volatile uint32_t overrun_count;
  /** @brief 事件存储区，深度 256 */
  bsp_uart_dmx_event_t events[BSP_UART_DMX_FIFO_SIZE];
} bsp_uart_dmx_fifo_t;

/** @brief 全局唯一软件 FIFO 实例，静态分配于 .bss 段 */
static bsp_uart_dmx_fifo_t s_bsp_uart_dmx_fifo;
static volatile bsp_uart_dmx_stats_t s_bsp_uart_dmx_stats;

/**
 * @brief   重置软件 FIFO（仅初始化时调用）
 *
 * 将 head、tail 归零，溢出计数归零。
 * 不需要关闭中断，因为初始化阶段任务调度尚未启动。
 */
static void bsp_uart_dmx_fifo_reset(void)
{
  s_bsp_uart_dmx_fifo.head = 0U;
  s_bsp_uart_dmx_fifo.tail = 0U;
  s_bsp_uart_dmx_fifo.overrun_count = 0U;
  s_bsp_uart_dmx_stats.irq_count = 0U;
  s_bsp_uart_dmx_stats.rx_bytes = 0U;
  s_bsp_uart_dmx_stats.break_count = 0U;
  s_bsp_uart_dmx_stats.ferr_count = 0U;
  s_bsp_uart_dmx_stats.nerr_count = 0U;
  s_bsp_uart_dmx_stats.roerr_count = 0U;
  s_bsp_uart_dmx_stats.fifo_overruns = 0U;
}

/**
 * @brief   向软件 FIFO 压入一个事件（中断安全，仅限 ISR 调用）
 *
 * @param[in] byte     接收到的字节
 * @param[in] is_break 是否为 Break 信号事件
 *
 * @note    本函数仅在 USART1 中断处理程序中调用，
 *          不需要额外加锁（单一生产者模型）
 */
static void bsp_uart_dmx_fifo_push_isr(uint8_t byte, bool is_break)
{
  uint16_t head;
  uint16_t next_head;

  head = s_bsp_uart_dmx_fifo.head;
  next_head = (uint16_t)((head + 1U) & BSP_UART_DMX_FIFO_MASK);

  /* FIFO 已满，丢弃最旧的数据并计数 */
  if(next_head == s_bsp_uart_dmx_fifo.tail)
  {
    s_bsp_uart_dmx_fifo.overrun_count++;
    s_bsp_uart_dmx_stats.fifo_overruns++;
    return;
  }

  s_bsp_uart_dmx_fifo.events[head].byte = byte;
  s_bsp_uart_dmx_fifo.events[head].is_break = is_break;
  s_bsp_uart_dmx_fifo.head = next_head;
}

void bsp_uart_dmx_init(void)
{
  bsp_uart_dmx_fifo_reset();
}

/**
 * @brief   USART1 DMX 中断处理程序
 *
 * 处理所有与 DMX 接收相关的中断标志：
 *
 * ## 第一分支：错误标志置位（Break 信号检测路径）
 *   当 FERR / BFF / NERR / ROERR 任一标志置位时进入
 *   - 清除所有错误标志
 *   - 读取并丢弃 RDBF 中的假字节（Break 期间的错误数据）
 *   - 如果检测到 Break（has_break == true），向 FIFO 压入 Break 事件
 *
 * ## 第二分支：正常字节接收（RDBF 置位且无错误）
 *   读取 USART 数据寄存器，将正常字节压入 FIFO，is_break = false
 *
 * @warning 不得在中断处理函数中执行耗时操作（如 memcpy、printf）
 * @note    中断入口在 at32f415_int.c 的 USART1_IRQHandler()
 */
void bsp_uart_dmx_irq_handler(void)
{
  bool ferr;
  bool nerr;
  bool roerr;
  bool bff;
  bool rdbf;
  bool has_break;

  /* 汇总所有错误标志：帧错误 + Break + 噪声 + 溢出 */
  s_bsp_uart_dmx_stats.irq_count++;
  ferr = (usart_flag_get(USART1, USART_FERR_FLAG) == SET);
  nerr = (usart_flag_get(USART1, USART_NERR_FLAG) == SET);
  roerr = (usart_flag_get(USART1, USART_ROERR_FLAG) == SET);
  bff = (usart_flag_get(USART1, USART_BFF_FLAG) == SET);
  rdbf = (usart_flag_get(USART1, USART_RDBF_FLAG) == SET);
  has_break = ferr;

  if(ferr) { s_bsp_uart_dmx_stats.ferr_count++; }
  if(nerr) { s_bsp_uart_dmx_stats.nerr_count++; }
  if(roerr) { s_bsp_uart_dmx_stats.roerr_count++; }

  /* ---- 分支一：发生错误（通常为 Break 信号期间的低电平误判） ---- */
  if(ferr || nerr || roerr)
  {
    if(rdbf)
    {
      s_bsp_uart_dmx_stats.rx_bytes++;
    }
    usart_flag_clear(USART1, USART_FERR_FLAG | USART_NERR_FLAG | USART_ROERR_FLAG);

    /* 读取并丢弃 RDBF 中的假字节（Break 期间接收到的垃圾数据）*/
    /* 识别到 Break 信号：向 FIFO 压入 Break 标记事件 */
    if(has_break)
    {
      s_bsp_uart_dmx_stats.break_count++;
      bsp_uart_dmx_fifo_push_isr(0U, true);
    }
    return;
  }

  /* ---- 分支二：正常字节接收 ---- */
  if(usart_flag_get(USART1, USART_RDBF_FLAG) == SET)
  {
    s_bsp_uart_dmx_stats.rx_bytes++;
    bsp_uart_dmx_fifo_push_isr((uint8_t)usart_data_receive(USART1), false);
  }
}

/**
 * @brief   从软件 FIFO 读取一个 DMX 事件（任务级调用）
 *
 * @param[out] out  读取到的事件写入此指针
 * @return         读取是否成功
 *
 * @note    使用临界区（关闭全局中断）保护共享数据，
 *          确保与中断处理程序之间的数据一致性
 */
bool bsp_uart_dmx_poll_event(bsp_uart_dmx_event_t *out)
{
  uint16_t tail;

  if(out == 0)
  {
    return false;
  }

  /* 进入临界区：防止与中断处理程序并发访问 */
  __disable_irq();
  tail = s_bsp_uart_dmx_fifo.tail;

  /* FIFO 为空（head == tail） */
  if(tail == s_bsp_uart_dmx_fifo.head)
  {
    __enable_irq();
    return false;
  }

  /* 复制事件数据并移动读指针 */
  *out = s_bsp_uart_dmx_fifo.events[tail];
  s_bsp_uart_dmx_fifo.tail = (uint16_t)((tail + 1U) & BSP_UART_DMX_FIFO_MASK);
  __enable_irq();

  return true;
}

void bsp_uart_dmx_get_stats(bsp_uart_dmx_stats_t *out)
{
  if(out == 0)
  {
    return;
  }

  __disable_irq();
  out->irq_count = s_bsp_uart_dmx_stats.irq_count;
  out->rx_bytes = s_bsp_uart_dmx_stats.rx_bytes;
  out->break_count = s_bsp_uart_dmx_stats.break_count;
  out->ferr_count = s_bsp_uart_dmx_stats.ferr_count;
  out->nerr_count = s_bsp_uart_dmx_stats.nerr_count;
  out->roerr_count = s_bsp_uart_dmx_stats.roerr_count;
  out->fifo_overruns = s_bsp_uart_dmx_stats.fifo_overruns;
  __enable_irq();
}
