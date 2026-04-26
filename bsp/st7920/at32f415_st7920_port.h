#ifndef BSP_ST7920_AT32F415_ST7920_PORT_H
#define BSP_ST7920_AT32F415_ST7920_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool at32_st7920_init(void);
bool at32_st7920_flush_page_buffer(const uint8_t *page_buffer_128x64);

#endif
