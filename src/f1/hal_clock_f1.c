/**
 * @file hal_clock_f1.c
 * @brief STM32F1 family-specific clock configuration
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define HSI_HZ 8000000U
#define HSE_HZ 8000000U

static volatile uint32_t ms_ticks = 0;

static uint32_t sysclk_hz = HSI_HZ;
static uint32_t hclk_hz = HSI_HZ;
static uint32_t pclk1_hz = HSI_HZ;
static uint32_t pclk2_hz = HSI_HZ;

const Clock_Config CLOCK_CONFIG_72MHZ_HSE = {
    .source = CLOCK_SOURCE_HSE,
    .pll_mul = 9,
    .ahb_div = CLOCK_AHB_DIV1,
    .apb1_div = CLOCK_APB_DIV2,
    .apb2_div = CLOCK_APB_DIV1,
};

static const uint32_t ahb_cfgr_bits[] = {
    RCC_CFGR_HPRE_DIV1,   RCC_CFGR_HPRE_DIV2,   RCC_CFGR_HPRE_DIV4,
    RCC_CFGR_HPRE_DIV8,   RCC_CFGR_HPRE_DIV16,  RCC_CFGR_HPRE_DIV64,
    RCC_CFGR_HPRE_DIV128, RCC_CFGR_HPRE_DIV256, RCC_CFGR_HPRE_DIV512};

static const uint16_t ahb_div_values[] = {1, 2, 4, 8, 16, 64, 128, 256, 512};

static const uint32_t apb1_cfgr_bits[] = {
    RCC_CFGR_PPRE1_DIV1, RCC_CFGR_PPRE1_DIV2, RCC_CFGR_PPRE1_DIV4,
    RCC_CFGR_PPRE1_DIV8, RCC_CFGR_PPRE1_DIV16};

static const uint32_t apb2_cfgr_bits[] = {
    RCC_CFGR_PPRE2_DIV1, RCC_CFGR_PPRE2_DIV2, RCC_CFGR_PPRE2_DIV4,
    RCC_CFGR_PPRE2_DIV8, RCC_CFGR_PPRE2_DIV16};

static const uint8_t apb_div_values[] = {1, 2, 4, 8, 16};

uint32_t hal_clock_flash_latency_for(uint32_t sysclk) {
  if (sysclk <= 24000000U) {
    return FLASH_ACR_LATENCY_0;
  }
  if (sysclk <= 48000000U) {
    return FLASH_ACR_LATENCY_1;
  }
  return FLASH_ACR_LATENCY_2;
}

uint32_t hal_clock_sysclk_for(const Clock_Config *cfg) {
  uint32_t src_hz = (cfg->source == CLOCK_SOURCE_HSE) ? HSE_HZ : HSI_HZ;
  return src_hz * cfg->pll_mul;
}

void hal_sysclock_init(const Clock_Config *cfg) {
  sysclk_hz = hal_clock_sysclk_for(cfg);
  hclk_hz = sysclk_hz / ahb_div_values[cfg->ahb_div];
  pclk1_hz = hclk_hz / apb_div_values[cfg->apb1_div];
  pclk2_hz = hclk_hz / apb_div_values[cfg->apb2_div];

  if (cfg->source == CLOCK_SOURCE_HSE) {
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
    }
  } else {
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
    }
  }

  FLASH->ACR |= FLASH_ACR_PRFTBE;
  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR |= hal_clock_flash_latency_for(sysclk_hz);

  RCC->CFGR &= ~RCC_CFGR_HPRE;
  RCC->CFGR |= ahb_cfgr_bits[cfg->ahb_div];

  RCC->CFGR &= ~RCC_CFGR_PPRE1;
  RCC->CFGR |= apb1_cfgr_bits[cfg->apb1_div];

  RCC->CFGR &= ~RCC_CFGR_PPRE2;
  RCC->CFGR |= apb2_cfgr_bits[cfg->apb2_div];

  RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
  if (cfg->source == CLOCK_SOURCE_HSE) {
    RCC->CFGR |= RCC_CFGR_PLLSRC;
  }
  RCC->CFGR |= ((uint32_t)(cfg->pll_mul - 2U) << RCC_CFGR_PLLMULL_Pos);

  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY)) {
  }

  RCC->CFGR &= ~RCC_CFGR_SW;
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }
}

void hal_sysclock_deinit(void) {
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) {
  }
  RCC->CFGR &= ~RCC_CFGR_SW;
  RCC->CFGR |= RCC_CFGR_SW_HSI;

  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
  }

  RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
  RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR &= ~FLASH_ACR_PRFTBE;

  sysclk_hz = HSI_HZ;
  hclk_hz = HSI_HZ;
  pclk1_hz = HSI_HZ;
  pclk2_hz = HSI_HZ;
}

uint32_t hal_clock_get_sysclk_hz(void) { return sysclk_hz; }
uint32_t hal_clock_get_hclk_hz(void) { return hclk_hz; }
uint32_t hal_clock_get_pclk1_hz(void) { return pclk1_hz; }
uint32_t hal_clock_get_pclk2_hz(void) { return pclk2_hz; }

uint32_t hal_clock_get_timer_clk_hz(const TIM_TypeDef *tim) {
  if ((tim == TIM2) || (tim == TIM3) || (tim == TIM4)) {
    uint32_t apb1_div = hclk_hz / pclk1_hz;
    return (apb1_div == 1U) ? pclk1_hz : (pclk1_hz * 2U);
  }

  uint32_t apb2_div = hclk_hz / pclk2_hz;
  return (apb2_div == 1U) ? pclk2_hz : (pclk2_hz * 2U);
}

void hal_systick_init(void) {
  SysTick->LOAD = (hal_clock_get_sysclk_hz() / 1000U) - 1U;
  SysTick->VAL = 0x00;
  SysTick->CTRL = (1 << 2) | (1 << 1) | (1 << 0);
}

void SysTick_Handler(void) { ms_ticks++; }

uint32_t hal_get_tick(void) { return ms_ticks; }

void hal_delay_ms(const uint32_t delay_ms) {
  uint32_t start = ms_ticks;
  while ((ms_ticks - start) < delay_ms) {
  }
}

void hal_gpio_init_port_clock(GPIO_TypeDef *port) {
  if (port == GPIOA) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  } else if (port == GPIOB) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
  } else if (port == GPIOC) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
  }
}

void hal_gpio_deinit_port_clock(GPIO_TypeDef *port) {
  if (port == GPIOA) {
    RCC->APB2ENR &= ~RCC_APB2ENR_IOPAEN;
  } else if (port == GPIOB) {
    RCC->APB2ENR &= ~RCC_APB2ENR_IOPBEN;
  } else if (port == GPIOC) {
    RCC->APB2ENR &= ~RCC_APB2ENR_IOPCEN;
  }
}
