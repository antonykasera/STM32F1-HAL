/**
 * @file hal_capture_f1.c
 * @brief STM32F1 pulse measurement in PWM input mode, TIM2/3/4
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_capture.h"
#include "hal_clock.h"
#include "hal_error.h"
#include "hal_timer.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define CAP_CCS_TI1 1U        /* CC1S = 01: IC1 on TI1 */
#define CAP_CCS_TI1_CROSS 2U  /* CC2S = 10: IC2 also on TI1 */
#define CAP_TS_TI1FP1 5U      /* trigger = TI1FP1 */
#define CAP_SMS_RESET 4U      /* slave reset mode */

HAL_Status hal_capture_calc_psc(uint32_t timer_clk_hz, uint32_t tick_hz,
                                uint16_t *psc) {
  if ((tick_hz == 0U) || (psc == NULL) || (tick_hz > timer_clk_hz) ||
      ((timer_clk_hz % tick_hz) != 0U)) {
    return HAL_ERR_PARAM;
  }
  uint32_t div = timer_clk_hz / tick_hz;
  if (div > 65536U) {
    return HAL_ERR_PARAM;
  }
  *psc = (uint16_t)(div - 1U);
  return HAL_OK;
}

HAL_Status hal_capture_init(Capture *c, TIM_TypeDef *tim, uint32_t tick_hz) {
  uint16_t psc = 0U;
  HAL_Status st =
      hal_capture_calc_psc(hal_clock_get_timer_clk_hz(tim), tick_hz, &psc);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return st;
  }

  st = hal_timer_base_config(tim, psc, 0xFFFFU);
  if (st != HAL_OK) {
    return st;
  }

  tim->CR1 &= ~TIM_CR1_CEN;
  tim->CCMR1 = (CAP_CCS_TI1 << TIM_CCMR1_CC1S_Pos) |
               (CAP_CCS_TI1_CROSS << TIM_CCMR1_CC2S_Pos);
  tim->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P; /* CC2: falling */
  tim->SMCR = (CAP_TS_TI1FP1 << TIM_SMCR_TS_Pos) |
              (CAP_SMS_RESET << TIM_SMCR_SMS_Pos);

  c->tim = tim;
  c->tick_hz = tick_hz;
  tim->CR1 |= TIM_CR1_CEN;
  return HAL_OK;
}

/* the counter runs 0..CCRx before the reset edge, so +1 (RM0008 Fig. 128) */
HAL_Status hal_capture_try_read(Capture *c, uint32_t *period, uint32_t *high) {
  if ((c->tim->SR & TIM_SR_CC1IF) == 0U) {
    return HAL_ERR_EMPTY;
  }
  uint32_t ccr2 = c->tim->CCR2;
  uint32_t ccr1 = c->tim->CCR1; /* reading CCR1 clears CC1IF */

  *period = ccr1 + 1U;
  *high = ccr2 + 1U;
  return HAL_OK;
}
