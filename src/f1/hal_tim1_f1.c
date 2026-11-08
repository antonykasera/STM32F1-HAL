/**
 * @file hal_tim1_f1.c
 * @brief STM32F1 TIM1 advanced PWM driver
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_pwm.h"
#include "hal_tim1.h"
#include "hal_timer.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

HAL_Status hal_tim1_init(TIM_TypeDef *tim, const TIM1_Config *cfg) {
  HAL_ASSERT(tim == TIM1);
  if (tim != TIM1) {
    return HAL_ERR_UNSUPPORTED;
  }

  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  uint32_t clk = hal_clock_get_timer_clk_hz(tim);

  uint16_t psc = 0U;
  uint16_t arr = 0U;
  HAL_Status st = hal_timer_solve_psc_arr(clk, cfg->freq_hz, &psc, &arr);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return st;
  }

  uint8_t dtg = 0U;
  uint32_t actual_ns = 0U;
  st = hal_tim1_calc_dtg(clk, cfg->dead_ns, &dtg, &actual_ns);
  HAL_ASSERT(st == HAL_OK); /* longer than 1008 timer ticks */
  if (st != HAL_OK) {
    return st;
  }

  tim->CR1 = TIM_CR1_ARPE | TIM_CR1_URS; /* edge-aligned, up, stopped */
  tim->PSC = psc;
  tim->ARR = arr;
  tim->RCR = 0U;
  tim->BDTR = hal_tim1_calc_bdtr(dtg, cfg->break_enable); /* one write, MOE off */
  tim->EGR = TIM_EGR_UG;
  tim->SR = ~(uint32_t)(TIM_SR_UIF | TIM_SR_BIF);
  return HAL_OK;
}

void hal_tim1_channel_init(TIM_TypeDef *tim, PWM_Channel ch,
                           bool complementary) {
  HAL_ASSERT(!(complementary && (ch == PWM_CH4))); /* CH4 has no CH4N */

  hal_pwm_channel_init(tim, ch);
  if (complementary) {
    tim->CCER |= (uint32_t)(TIM_CCER_CC1NE << ((uint32_t)ch * 4U));
  }
}

/* MOE can't be set while BKIN is still active (RM0008), so after a break this
 * only takes effect once the input is released */
void hal_tim1_start(TIM_TypeDef *tim) {
  tim->SR = ~(uint32_t)TIM_SR_BIF; /* rc_w0 */
  tim->BDTR |= TIM_BDTR_MOE;
  tim->CR1 |= TIM_CR1_CEN;
}

void hal_tim1_stop(TIM_TypeDef *tim) {
  tim->BDTR &= ~TIM_BDTR_MOE;
  tim->CR1 &= ~TIM_CR1_CEN;
}

bool hal_tim1_tripped(const TIM_TypeDef *tim) {
  return (tim->SR & TIM_SR_BIF) != 0U;
}
