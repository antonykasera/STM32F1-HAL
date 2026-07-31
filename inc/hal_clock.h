/**
 * @file hal_clock.h
 * @brief Public clock configuration API
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include "hal_types.h"
#include "stm32f103xb.h"
#include <stdint.h>

typedef enum { CLOCK_SOURCE_HSI, CLOCK_SOURCE_HSE } Clock_Source;

typedef enum {
  CLOCK_AHB_DIV1,
  CLOCK_AHB_DIV2,
  CLOCK_AHB_DIV4,
  CLOCK_AHB_DIV8,
  CLOCK_AHB_DIV16,
  CLOCK_AHB_DIV64,
  CLOCK_AHB_DIV128,
  CLOCK_AHB_DIV256,
  CLOCK_AHB_DIV512
} Clock_AHB_Prescaler;

typedef enum {
  CLOCK_APB_DIV1,
  CLOCK_APB_DIV2,
  CLOCK_APB_DIV4,
  CLOCK_APB_DIV8,
  CLOCK_APB_DIV16
} Clock_APB_Prescaler;

typedef struct {
  Clock_Source source;
  uint8_t pll_mul;
  Clock_AHB_Prescaler ahb_div;
  Clock_APB_Prescaler apb1_div;
  Clock_APB_Prescaler apb2_div;
} Clock_Config;

extern const Clock_Config CLOCK_CONFIG_72MHZ_HSE;

uint32_t hal_clock_flash_latency_for(uint32_t sysclk_hz);
uint32_t hal_clock_sysclk_for(const Clock_Config *cfg);

void hal_sysclock_init(const Clock_Config *cfg);
void hal_sysclock_deinit(void);

uint32_t hal_clock_get_sysclk_hz(void);
uint32_t hal_clock_get_hclk_hz(void);
uint32_t hal_clock_get_pclk1_hz(void);
uint32_t hal_clock_get_pclk2_hz(void);
uint32_t hal_clock_get_timer_clk_hz(const TIM_TypeDef *tim);

void hal_systick_init(void);
uint32_t hal_get_tick(void);
void hal_delay_ms(uint32_t delay_ms);

void hal_gpio_init_port_clock(GPIO_TypeDef *port);
void hal_gpio_deinit_port_clock(GPIO_TypeDef *port);

#endif /* HAL_CLOCK_H */
