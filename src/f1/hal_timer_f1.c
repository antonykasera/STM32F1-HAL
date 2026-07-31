/**
 * @file hal_timer_f1.c
 * @brief STM32F1 TIMER implementation
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_timer.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define TIMER_DEFAULT_PRIORITY 6U
#define TIMER_COUNT 3

static timer_callback_t timer_callbacks[TIMER_COUNT];

static const IRQn_Type timer_irq_vectors[TIMER_COUNT] = {TIM2_IRQn, TIM3_IRQn,
                                                         TIM4_IRQn};

static const uint32_t timer_rcc_masks[TIMER_COUNT] = {
    RCC_APB1ENR_TIM2EN, RCC_APB1ENR_TIM3EN, RCC_APB1ENR_TIM4EN};

static int timer_index(TIM_TypeDef *tim) {
  if (tim == TIM2) {
    return 0;
  }
  if (tim == TIM3) {
    return 1;
  }
  if (tim == TIM4) {
    return 2;
  }
  return -1;
}

static void timer_dispatch(TIM_TypeDef *tim) {
  int idx = timer_index(tim);

  tim->SR &= ~TIM_SR_UIF;

  if ((idx >= 0) && (timer_callbacks[idx] != NULL)) {
    timer_callbacks[idx]();
  }
}

void hal_timer_init(TIM_TypeDef *tim, uint16_t psc, uint16_t arr) {
  int idx = timer_index(tim);
  if (idx < 0) {
    return;
  }

  RCC->APB1ENR |= timer_rcc_masks[idx];

  tim->PSC = psc;
  tim->ARR = arr;

  tim->CR1 |= TIM_CR1_URS;
  tim->EGR = TIM_EGR_UG;
  tim->SR &= ~TIM_SR_UIF;

  tim->DIER |= TIM_DIER_UIE;

  hal_timer_set_priority(tim, TIMER_DEFAULT_PRIORITY);
  __NVIC_EnableIRQ(timer_irq_vectors[idx]);
}

void hal_timer_init_hz(TIM_TypeDef *tim, uint32_t freq_hz) {
  if (freq_hz == 0U) {
    return;
  }

  uint16_t psc = 0U;
  uint16_t arr = 0U;
  hal_timer_solve_psc_arr(hal_clock_get_timer_clk_hz(tim), freq_hz, &psc, &arr);

  hal_timer_init(tim, psc, arr);
}

void hal_timer_deinit(TIM_TypeDef *tim) {
  int idx = timer_index(tim);
  if (idx < 0) {
    return;
  }

  tim->CR1 &= ~TIM_CR1_CEN;
  tim->DIER &= ~TIM_DIER_UIE;
  tim->SR &= ~TIM_SR_UIF;

  __NVIC_DisableIRQ(timer_irq_vectors[idx]);

  timer_callbacks[idx] = NULL;
}

void hal_timer_start(TIM_TypeDef *tim) { tim->CR1 |= TIM_CR1_CEN; }

void hal_timer_stop(TIM_TypeDef *tim) { tim->CR1 &= ~TIM_CR1_CEN; }

void hal_timer_register_callback(TIM_TypeDef *tim, timer_callback_t cb) {
  int idx = timer_index(tim);
  if (idx >= 0) {
    timer_callbacks[idx] = cb;
  }
}

void hal_timer_set_priority(TIM_TypeDef *tim, uint8_t priority) {
  int idx = timer_index(tim);
  if (idx >= 0) {
    __NVIC_SetPriority(timer_irq_vectors[idx], priority);
  }
}

void TIM2_IRQHandler(void) { timer_dispatch(TIM2); }
void TIM3_IRQHandler(void) { timer_dispatch(TIM3); }
void TIM4_IRQHandler(void) { timer_dispatch(TIM4); }
