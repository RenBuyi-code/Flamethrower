/* add user code begin Header */
/**
  **************************************************************************
  * @file     wk_usart.c
  * @brief    work bench config program
  **************************************************************************
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */
/* add user code end Header */

/* Includes ------------------------------------------------------------------*/
#include "wk_usart.h"
#include "../../bsp/at32f415/bsp_uart.h"

/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  init usart1 function
  * @param  none
  * @retval none
  */
void wk_usart1_init(void)
{
  /* add user code begin usart1_init 0 */

  /* add user code end usart1_init 0 */

  gpio_init_type gpio_init_struct;
  gpio_default_para_init(&gpio_init_struct);

  /* add user code begin usart1_init 1 */

  /* add user code end usart1_init 1 */

  /* configure the RX pin */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = DMX_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(DMX_RX_GPIO_PORT, &gpio_init_struct);

  /* configure param */
  usart_init(USART1, 250000, USART_DATA_8BITS, USART_STOP_2_BIT);
  usart_receiver_enable(USART1, TRUE);
  usart_parity_selection_config(USART1, USART_PARITY_NONE);

  usart_hardware_flow_control_set(USART1, USART_HARDWARE_FLOW_NONE);

  /* add user code begin usart1_init 2 */
  bsp_uart_dmx_init();
  usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
  usart_interrupt_enable(USART1, USART_ERR_INT, TRUE);
  NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 6, 0));
  NVIC_EnableIRQ(USART1_IRQn);

  /* add user code end usart1_init 2 */
  
  usart_enable(USART1, TRUE);

  /* add user code begin usart1_init 3 */

  /* add user code end usart1_init 3 */
}

/* add user code begin 1 */

/* add user code end 1 */
