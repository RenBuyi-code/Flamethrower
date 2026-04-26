/* add user code begin Header */
/**
  **************************************************************************
  * @file     at32f415_wk_config.h
  * @brief    header file of work bench config
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

/* define to prevent recursive inclusion -----------------------------------*/
#ifndef __AT32F415_WK_CONFIG_H
#define __AT32F415_WK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* includes -----------------------------------------------------------------------*/
#include "stdio.h"
#include "at32f415.h"

/* private includes -------------------------------------------------------------*/
/* add user code begin private includes */

/* add user code end private includes */

/* exported types -------------------------------------------------------------*/
/* add user code begin exported types */

/* add user code end exported types */

/* exported constants --------------------------------------------------------*/
/* add user code begin exported constants */

/* add user code end exported constants */

/* exported macro ------------------------------------------------------------*/
/* add user code begin exported macro */

/* add user code end exported macro */

/* add user code begin dma define */
/* user can only modify the dma define value */
//#define DMA1_CHANNEL1_BUFFER_SIZE   0
//#define DMA1_CHANNEL1_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL1_PERIPHERAL_BASE_ADDR  0

//#define DMA1_CHANNEL2_BUFFER_SIZE   0
//#define DMA1_CHANNEL2_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL2_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL3_BUFFER_SIZE   0
//#define DMA1_CHANNEL3_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL3_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL4_BUFFER_SIZE   0
//#define DMA1_CHANNEL4_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL4_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL5_BUFFER_SIZE   0
//#define DMA1_CHANNEL5_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL5_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL6_BUFFER_SIZE   0
//#define DMA1_CHANNEL6_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL6_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL7_BUFFER_SIZE   0
//#define DMA1_CHANNEL7_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL7_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL1_BUFFER_SIZE   0
//#define DMA2_CHANNEL1_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL1_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL2_BUFFER_SIZE   0
//#define DMA2_CHANNEL2_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL2_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL3_BUFFER_SIZE   0
//#define DMA2_CHANNEL3_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL3_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL4_BUFFER_SIZE   0
//#define DMA2_CHANNEL4_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL4_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL5_BUFFER_SIZE   0
//#define DMA2_CHANNEL5_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL5_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL6_BUFFER_SIZE   0
//#define DMA2_CHANNEL6_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL6_PERIPHERAL_BASE_ADDR   0

//#define DMA2_CHANNEL7_BUFFER_SIZE   0
//#define DMA2_CHANNEL7_MEMORY_BASE_ADDR   0
//#define DMA2_CHANNEL7_PERIPHERAL_BASE_ADDR   0
/* add user code end dma define */

/* Private defines -------------------------------------------------------------*/
#define ADC_PRESSURE_PIN    GPIO_PINS_0
#define ADC_PRESSURE_GPIO_PORT    GPIOA
#define O_LED_PIN    GPIO_PINS_2
#define O_LED_GPIO_PORT    GPIOA
#define LED_POWER_PIN    GPIO_PINS_3
#define LED_POWER_GPIO_PORT    GPIOA
#define ADC_PW1_PIN    GPIO_PINS_4
#define ADC_PW1_GPIO_PORT    GPIOA
#define ADC_PW2_PIN    GPIO_PINS_5
#define ADC_PW2_GPIO_PORT    GPIOA
#define ADC_PW3_PIN    GPIO_PINS_6
#define ADC_PW3_GPIO_PORT    GPIOA
#define KEY_MENU_PIN    GPIO_PINS_7
#define KEY_MENU_GPIO_PORT    GPIOA
#define KEY_DOWN_PIN    GPIO_PINS_0
#define KEY_DOWN_GPIO_PORT    GPIOB
#define KEY_UP_PIN    GPIO_PINS_1
#define KEY_UP_GPIO_PORT    GPIOB
#define KEY_ENTER_PIN    GPIO_PINS_2
#define KEY_ENTER_GPIO_PORT    GPIOB
#define LED_OIL_PUMP_PIN    GPIO_PINS_10
#define LED_OIL_PUMP_GPIO_PORT    GPIOB
#define LED_ERR_PIN    GPIO_PINS_11
#define LED_ERR_GPIO_PORT    GPIOB
#define DRV_OIL_PUMP_PIN    GPIO_PINS_12
#define DRV_OIL_PUMP_GPIO_PORT    GPIOB
#define DRV_OIL_LOCK_SV_PIN    GPIO_PINS_14
#define DRV_OIL_LOCK_SV_GPIO_PORT    GPIOB
#define DRV_RV_PIN    GPIO_PINS_15
#define DRV_RV_GPIO_PORT    GPIOB
#define DRV_IGNITER_PIN    GPIO_PINS_8
#define DRV_IGNITER_GPIO_PORT    GPIOA
#define DMX_RX_PIN    GPIO_PINS_10
#define DMX_RX_GPIO_PORT    GPIOA
#define IO_IN_WIRELESS_PIN    GPIO_PINS_12
#define IO_IN_WIRELESS_GPIO_PORT    GPIOA
#define IO_IN_TILT_SW_PIN    GPIO_PINS_15
#define IO_IN_TILT_SW_GPIO_PORT    GPIOA
#define LCD_SCK_PIN    GPIO_PINS_4
#define LCD_SCK_GPIO_PORT    GPIOB
#define LCD_SID_PIN    GPIO_PINS_5
#define LCD_SID_GPIO_PORT    GPIOB
#define LCD_CS_PIN    GPIO_PINS_6
#define LCD_CS_GPIO_PORT    GPIOB
#define LCD_BL_PIN    GPIO_PINS_7
#define LCD_BL_GPIO_PORT    GPIOB
#define LED_DMX_PIN    GPIO_PINS_8
#define LED_DMX_GPIO_PORT    GPIOB
#define LED_RX_PIN    GPIO_PINS_9
#define LED_RX_GPIO_PORT    GPIOB

/* exported functions ------------------------------------------------------- */
  /* system clock config. */
  void wk_system_clock_config(void);

  /* config periph clock. */
  void wk_periph_clock_config(void);

  /* nvic config. */
  void wk_nvic_config(void);

/* add user code begin exported functions */

/* add user code end exported functions */

#ifdef __cplusplus
}
#endif

#endif
