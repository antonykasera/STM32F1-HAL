/**
 * @file hal_tim1_calc.c
 * @brief TIM1 dead-time and break register arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * Dead time in timer ticks (CKD = 0, RM0008 BDTR.DTG):
 *   DTG = 0xxxxxxx -> DTG                 (0..127, step 1)
 *   DTG = 10xxxxxx -> (64 + DTG[5:0]) * 2   (128..254, step 2)
 *   DTG = 110xxxxx -> (32 + DTG[4:0]) * 8   (256..504, step 8)
 *   DTG = 111xxxxx -> (32 + DTG[4:0]) * 16  (512..1008, step 16)
 */

#include "hal_tim1.h"
#include <stddef.h>
#include <stdint.h>

static uint32_t div_up(uint32_t a, uint32_t b) { return (a + b - 1U) / b; }

HAL_Status hal_tim1_calc_dtg(uint32_t timer_clk_hz, uint32_t dead_ns,
                             uint8_t *dtg, uint32_t *actual_ns) {
  if ((timer_clk_hz == 0U) || (dtg == NULL) || (actual_ns == NULL)) {
    return HAL_ERR_PARAM;
  }

  /* round up: a dead time shorter than asked could let a bridge shoot through */
  uint64_t num = (uint64_t)dead_ns * timer_clk_hz;
  uint32_t ticks = (uint32_t)((num + 999999999U) / 1000000000U);
  uint32_t code;
  uint32_t dt;

  if (ticks <= 127U) {
    code = ticks;
    dt = ticks;
  } else if (ticks <= 254U) {
    uint32_t n = div_up(ticks, 2U) - 64U;
    code = 0x80U | n;
    dt = (64U + n) * 2U;
  } else if (ticks <= 504U) {
    uint32_t n = div_up(ticks, 8U);
    n = (n > 32U) ? (n - 32U) : 0U;
    code = 0xC0U | n;
    dt = (32U + n) * 8U;
  } else if (ticks <= 1008U) {
    uint32_t n = div_up(ticks, 16U) - 32U;
    code = 0xE0U | n;
    dt = (32U + n) * 16U;
  } else {
    return HAL_ERR_PARAM;
  }

  *dtg = (uint8_t)code;
  *actual_ns = (uint32_t)((((uint64_t)dt * 1000000000U) + (timer_clk_hz / 2U)) /
                          timer_clk_hz);
  return HAL_OK;
}

uint32_t hal_tim1_calc_bdtr(uint8_t dtg, bool break_enable) {
  uint32_t bdtr = ((uint32_t)dtg << TIM_BDTR_DTG_Pos) | TIM_BDTR_OSSI |
                  TIM_BDTR_OSSR;

  if (break_enable) {
    bdtr |= TIM_BDTR_BKE; /* BKP = 0: active low */
  }
  return bdtr;
}
