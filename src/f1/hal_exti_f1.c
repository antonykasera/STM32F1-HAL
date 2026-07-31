/**
 * @file hal_exti_f1.c
 * @brief STM32F1 EXTI implementation
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_exti.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define EXTI_DEFAULT_PRIORITY 6U

static exti_callback_t exti_callbacks[16];

static const IRQn_Type exti_irq_vectors[16] = {
    EXTI0_IRQn,     EXTI1_IRQn,     EXTI2_IRQn,     EXTI3_IRQn,
    EXTI4_IRQn,     EXTI9_5_IRQn,   EXTI9_5_IRQn,   EXTI9_5_IRQn,
    EXTI9_5_IRQn,   EXTI9_5_IRQn,   EXTI15_10_IRQn, EXTI15_10_IRQn,
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn};

static uint8_t exti_port_index(const GPIO_TypeDef *port) {
  return (uint8_t)(((uint32_t)port - GPIOA_BASE) / 0x400U);
}

static void exti_dispatch(uint8_t line) {
  EXTI->PR = (1U << line);
  if (exti_callbacks[line] != NULL) {
    exti_callbacks[line]();
  }
}

void hal_exti_init(const GPIO_TypeDef *port, uint8_t pin, EXTI_Edge_t edge) {
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

  uint8_t reg_index = (uint8_t)(pin >> 2U);
  uint8_t bit_shift = (uint8_t)((pin & 0x03U) << 2U);
  uint8_t index = exti_port_index(port);

  AFIO->EXTICR[reg_index] &= ~(0xFU << bit_shift);
  AFIO->EXTICR[reg_index] |= ((uint32_t)index << bit_shift);

  EXTI->RTSR &= ~(1U << pin);
  EXTI->FTSR &= ~(1U << pin);

  if (edge == RISING_EDGE) {
    EXTI->RTSR |= (1U << pin);
  } else {
    EXTI->FTSR |= (1U << pin);
  }

  EXTI->IMR |= (1U << pin);
  hal_exti_set_priority(pin, EXTI_DEFAULT_PRIORITY);
  __NVIC_EnableIRQ(exti_irq_vectors[pin]);
}

void hal_exti_deinit(uint8_t line) {

  EXTI->PR = (1U << line);
  exti_callbacks[line] = NULL;
  EXTI->RTSR &= ~(1U << line);
  EXTI->FTSR &= ~(1U << line);

  EXTI->IMR &= ~(1U << line);
}

void hal_exti_register_callback(uint8_t line, exti_callback_t cb) {
  if (line < 16U) {
    exti_callbacks[line] = cb;
  }
}

void hal_exti_set_priority(uint8_t line, uint8_t priority) {
  if (line < 16U) {
    __NVIC_SetPriority(exti_irq_vectors[line], priority);
  }
}

void hal_exti_soft_trigger(uint8_t line) { EXTI->SWIER |= (1U << line); }

void hal_exti_enable(uint8_t line) { EXTI->IMR |= (1U << line); }

void hal_exti_disable(uint8_t line) { EXTI->IMR &= ~(1U << line); }

void EXTI0_IRQHandler(void) { exti_dispatch(0); }
void EXTI1_IRQHandler(void) { exti_dispatch(1); }
void EXTI2_IRQHandler(void) { exti_dispatch(2); }
void EXTI3_IRQHandler(void) { exti_dispatch(3); }
void EXTI4_IRQHandler(void) { exti_dispatch(4); }

void EXTI9_5_IRQHandler(void) {
  for (uint8_t line = 5; line <= 9; line++) {
    if (EXTI->PR & (1U << line)) {
      exti_dispatch(line);
    }
  }
}

void EXTI15_10_IRQHandler(void) {
  for (uint8_t line = 10; line <= 15; line++) {
    if (EXTI->PR & (1U << line)) {
      exti_dispatch(line);
    }
  }
}
