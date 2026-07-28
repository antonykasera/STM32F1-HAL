/**
 * @file hal_clock_f1.c
 * @brief STM32F1 family-specific clock configuration
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "board.h"
#include "hal_clock.h"
#include "stm32f103xb.h"
#include <stdint.h>

/* TODO: Implement clock init, PLL config, SysTick setup for F1 family */

static volatile uint32_t ms_ticks = 0;

/*NOTE: allow core clock parameter next, maybe implement some form of clock
 * config struct*/
void hal_sysclock_init(void) {

  RCC->CR |= RCC_CR_HSEON;
  while (!(RCC->CR & RCC_CR_HSERDY)) {
  }

  // FLASH Prefetch and Latency
  FLASH->ACR |= FLASH_ACR_PRFTBE;
  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR |= FLASH_ACR_LATENCY_2;

  // AHB Prescaler 72 MHz
  RCC->CFGR &= ~RCC_CFGR_HPRE;
  RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

  // APB1 Prescaler 36MHz
  RCC->CFGR &= ~RCC_CFGR_PPRE1;
  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

  // APB2 Prescaler 72MHz
  RCC->CFGR &= ~RCC_CFGR_PPRE2;
  RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

  RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
  RCC->CFGR |= RCC_CFGR_PLLSRC;
  RCC->CFGR |= RCC_CFGR_PLLMULL9;

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
}

void hal_systick_init(void) {
  SysTick->LOAD = (SYSCLK_HZ / 1000U) - 1;
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
