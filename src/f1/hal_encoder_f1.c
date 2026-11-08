/**
 * @file hal_encoder_f1.c
 * @brief STM32F1 quadrature encoder on TIM2/3/4
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_encoder.h"
#include "hal_error.h"
#include "hal_timer.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define ENC_SMS_BOTH_EDGES 3U /* encoder mode 3 */
#define ENC_CCS_TI 1U         /* CCxS = 01: ICx mapped on TIx */
#define ENC_FILTER_MAX 15U

int32_t hal_encoder_calc_delta(uint16_t now, uint16_t last) {
  int32_t d = (int32_t)(uint16_t)(now - last);
  return (d > 32767) ? (d - 65536) : d;
}

HAL_Status hal_encoder_init(Encoder *e, TIM_TypeDef *tim, uint8_t filter,
                            bool invert) {
  HAL_ASSERT(filter <= ENC_FILTER_MAX);

  HAL_Status st = hal_timer_base_config(tim, 0U, 0xFFFFU);
  if (st != HAL_OK) {
    return st;
  }

  tim->CR1 &= ~TIM_CR1_CEN;
  tim->SMCR = (tim->SMCR & ~TIM_SMCR_SMS) |
              (ENC_SMS_BOTH_EDGES << TIM_SMCR_SMS_Pos);
  tim->CCMR1 = (ENC_CCS_TI << TIM_CCMR1_CC1S_Pos) |
               (ENC_CCS_TI << TIM_CCMR1_CC2S_Pos) |
               ((uint32_t)filter << TIM_CCMR1_IC1F_Pos) |
               ((uint32_t)filter << TIM_CCMR1_IC2F_Pos);
  tim->CCER = invert ? TIM_CCER_CC1P : 0U; /* inverting A reverses direction */
  tim->CNT = 0U;

  e->tim = tim;
  e->last = 0U;
  e->position = 0;

  tim->CR1 |= TIM_CR1_CEN;
  return HAL_OK;
}

int32_t hal_encoder_update(Encoder *e) {
  uint16_t now = (uint16_t)e->tim->CNT;

  e->position += hal_encoder_calc_delta(now, e->last);
  e->last = now;
  return e->position;
}

void hal_encoder_set(Encoder *e, int32_t position) {
  e->last = (uint16_t)e->tim->CNT;
  e->position = position;
}
