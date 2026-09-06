#pragma once

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED

#define HSE_VALUE 8000000UL
#define HSE_STARTUP_TIMEOUT 100UL
#define HSI_VALUE 16000000UL
#define HSI48_VALUE 48000000UL
#define LSI_VALUE 32000UL
#define LSE_VALUE 32768UL
#define LSE_STARTUP_TIMEOUT 5000UL
#define EXTERNAL_CLOCK_VALUE 48000UL
#define VDD_VALUE 3300UL
#define TICK_INT_PRIORITY 15UL
#define USE_RTOS 0U
#define PREFETCH_ENABLE 0U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE 1U

#define assert_param(expression) ((void)0U)

// DMA handle declarations must precede the peripheral headers.
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32g4xx_hal_dma.h"
#endif
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_dac.h"
#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_pwr.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_tim.h"
#include "stm32g4xx_hal_uart.h"
