#include "bsp_uart.h"
#include "../../project/inc/at32f415_conf.h"

#define BSP_UART_DMX_FIFO_SIZE          256U
#define BSP_UART_DMX_FIFO_MASK          (BSP_UART_DMX_FIFO_SIZE - 1U)

typedef struct
{
  volatile uint16_t head;
  volatile uint16_t tail;
  volatile uint32_t overrun_count;
  bsp_uart_dmx_event_t events[BSP_UART_DMX_FIFO_SIZE];
} bsp_uart_dmx_fifo_t;

static bsp_uart_dmx_fifo_t s_bsp_uart_dmx_fifo;

static void bsp_uart_dmx_fifo_reset(void)
{
  s_bsp_uart_dmx_fifo.head = 0U;
  s_bsp_uart_dmx_fifo.tail = 0U;
  s_bsp_uart_dmx_fifo.overrun_count = 0U;
}

static void bsp_uart_dmx_fifo_push_isr(uint8_t byte, bool is_break)
{
  uint16_t head;
  uint16_t next_head;

  head = s_bsp_uart_dmx_fifo.head;
  next_head = (uint16_t)((head + 1U) & BSP_UART_DMX_FIFO_MASK);
  if(next_head == s_bsp_uart_dmx_fifo.tail)
  {
    s_bsp_uart_dmx_fifo.overrun_count++;
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

void bsp_uart_dmx_irq_handler(void)
{
  uint32_t err_flags;
  bool has_break;

  err_flags = USART_FERR_FLAG | USART_NERR_FLAG | USART_ROERR_FLAG | USART_BFF_FLAG;
  has_break = ((usart_flag_get(USART1, USART_FERR_FLAG) == SET) || (usart_flag_get(USART1, USART_BFF_FLAG) == SET));
  if((usart_flag_get(USART1, USART_FERR_FLAG) == SET)
     || (usart_flag_get(USART1, USART_BFF_FLAG) == SET)
     || (usart_flag_get(USART1, USART_NERR_FLAG) == SET)
     || (usart_flag_get(USART1, USART_ROERR_FLAG) == SET))
  {
    usart_flag_clear(USART1, err_flags);
    if(usart_flag_get(USART1, USART_RDBF_FLAG) == SET)
    {
      (void)usart_data_receive(USART1);
    }
    if(has_break)
    {
      bsp_uart_dmx_fifo_push_isr(0U, true);
    }
    return;
  }

  if(usart_flag_get(USART1, USART_RDBF_FLAG) == SET)
  {
    bsp_uart_dmx_fifo_push_isr((uint8_t)usart_data_receive(USART1), false);
  }
}

bool bsp_uart_dmx_poll_event(bsp_uart_dmx_event_t *out)
{
  uint16_t tail;

  if(out == 0)
  {
    return false;
  }

  __disable_irq();
  tail = s_bsp_uart_dmx_fifo.tail;
  if(tail == s_bsp_uart_dmx_fifo.head)
  {
    __enable_irq();
    return false;
  }

  *out = s_bsp_uart_dmx_fifo.events[tail];
  s_bsp_uart_dmx_fifo.tail = (uint16_t)((tail + 1U) & BSP_UART_DMX_FIFO_MASK);
  __enable_irq();
  return true;
}
