#ifndef BSP_AT32F415_BSP_UART_H
#define BSP_AT32F415_BSP_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t byte;
  bool is_break;
} bsp_uart_dmx_event_t;

void bsp_uart_dmx_init(void);
void bsp_uart_dmx_irq_handler(void);
bool bsp_uart_dmx_poll_event(bsp_uart_dmx_event_t *out);

#endif
