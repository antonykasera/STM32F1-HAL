/**
 * @file hal_pwm_f1.c
 * @brief STM32F1 PWM implementation (TIM2/3/4, edge-aligned PWM mode 1)
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_pwm.h"
#include "hal_timer.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PWM_OCM_MASK (0x7U << 4)
#define PWM_OCM_FORCE_INACTIVE (0x4U << 4)
#define PWM_OCM_PWM1 (0x6U << 4)
#define PWM_OCPE (0x1U << 3)
#define PWM_CH_BYTE_MASK 0xFFU

static volatile uint32_t *pwm_ccr(TIM_TypeDef *tim, PWM_Channel ch) {
  return (&tim->CCR1) + (uint32_t)ch;
}

static void pwm_config_ccmr(TIM_TypeDef *tim, PWM_Channel ch) {
  uint32_t byte = (PWM_OCM_PWM1 | PWM_OCPE);
  uint8_t shift = (uint8_t)(((uint32_t)ch & 1U) * 8U); /* low/high byte */

  if (ch <= PWM_CH2) {
    tim->CCMR1 &= ~((uint32_t)PWM_CH_BYTE_MASK << shift);
    tim->CCMR1 |= (byte << shift);
  } else {
    tim->CCMR2 &= ~((uint32_t)PWM_CH_BYTE_MASK << shift);
    tim->CCMR2 |= (byte << shift);
  }
}

void hal_pwm_init(TIM_TypeDef *tim, uint32_t freq_hz) {
  uint16_t psc = 0U;
  uint16_t arr = 0U;
  HAL_Status st = hal_timer_solve_psc_arr(hal_clock_get_timer_clk_hz(tim),
                                          freq_hz, &psc, &arr);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return;
  }

  if (hal_timer_base_config(tim, psc, arr) != HAL_OK) {
    return; /* unknown timer */
  }

  tim->CR1 |= TIM_CR1_ARPE;
}

void hal_pwm_channel_init(TIM_TypeDef *tim, PWM_Channel ch) {
  HAL_ASSERT(ch <= PWM_CH4);

  *pwm_ccr(tim, ch) = 0U; /* start at 0% duty */
  pwm_config_ccmr(tim, ch);
  tim->CCER |= (uint32_t)(TIM_CCER_CC1E << ((uint32_t)ch * 4U));

  /* CCR is preloaded; load it now only if stopped, UG would reset a running
   * counter (URS keeps UG from raising an update interrupt) */
  if ((tim->CR1 & TIM_CR1_CEN) == 0U) {
    tim->EGR = TIM_EGR_UG;
  }
}

void hal_pwm_set_duty_permille(TIM_TypeDef *tim, PWM_Channel ch,
                               uint16_t permille) {
  HAL_ASSERT(ch <= PWM_CH4);
  *pwm_ccr(tim, ch) = hal_pwm_calc_ccr(permille, (uint16_t)tim->ARR);
}

void hal_pwm_set_duty_raw(TIM_TypeDef *tim, PWM_Channel ch, uint16_t ccr) {
  HAL_ASSERT(ch <= PWM_CH4);
  *pwm_ccr(tim, ch) = ccr;
}

static void pwm_set_ocm(TIM_TypeDef *tim, PWM_Channel ch, uint32_t ocm) {
  uint8_t shift = (uint8_t)(((uint32_t)ch & 1U) * 8U);
  volatile uint32_t *ccmr = (ch <= PWM_CH2) ? &tim->CCMR1 : &tim->CCMR2;

  *ccmr = (*ccmr & ~((uint32_t)PWM_OCM_MASK << shift)) | (ocm << shift);
}

static bool pwm_channel_is_enabled(const TIM_TypeDef *tim, PWM_Channel ch) {
  return (tim->CCER & (TIM_CCER_CC1E << ((uint32_t)ch * 4U))) != 0U;
}

static void pwm_set_ocm_enabled(TIM_TypeDef *tim, uint32_t ocm) {
  for (uint32_t i = 0U; i < 4U; i++) {
    if (pwm_channel_is_enabled(tim, (PWM_Channel)i)) {
      pwm_set_ocm(tim, (PWM_Channel)i, ocm);
    }
  }
}

void hal_pwm_start(TIM_TypeDef *tim) {
  pwm_set_ocm_enabled(tim, PWM_OCM_PWM1);
  tim->CR1 |= TIM_CR1_CEN;
}

/* force outputs low first: clearing CEN alone freezes them, maybe mid-pulse */
void hal_pwm_stop(TIM_TypeDef *tim) {
  pwm_set_ocm_enabled(tim, PWM_OCM_FORCE_INACTIVE);
  tim->CR1 &= ~TIM_CR1_CEN;
}

void hal_pwm_channel_enable(TIM_TypeDef *tim, PWM_Channel ch, bool en) {
  HAL_ASSERT(ch <= PWM_CH4);
  uint32_t bit = (uint32_t)(TIM_CCER_CC1E << ((uint32_t)ch * 4U));
  if (en) {
    tim->CCER |= bit;
  } else {
    tim->CCER &= ~bit;
  }
}
