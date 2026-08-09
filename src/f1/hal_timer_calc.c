/**
 * @file hal_timer_calc.c
 * @brief Timer arithmetic
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_timer.h"
#include <stddef.h>
#include <stdint.h>

uint16_t hal_pwm_calc_ccr(uint16_t permille, uint16_t arr);

void hal_timer_solve_psc_arr(uint32_t timer_clk_hz, uint32_t freq_hz,
                             uint16_t *psc, uint16_t *arr) {
  if ((freq_hz == 0U) || (psc == NULL) || (arr == NULL)) {
    return;
  }

  uint32_t ticks = timer_clk_hz / freq_hz;
  uint32_t p = 0U;

  while ((ticks / (p + 1U)) > 65536U) {
    p++;
  }

  *psc = (uint16_t)p;
  *arr = (uint16_t)((ticks / (p + 1U)) - 1U);
}

/* Duty (parts-per-thousand) -> compare value. The period spans arr+1 counts, so
 * full scale (1000) maps to arr+1 (output high the entire period). Clamped so
 * values above 1000 saturate rather than overflow. */
uint16_t hal_pwm_calc_ccr(uint16_t permille, uint16_t arr) {
  if (permille >= 1000U) {
    return (uint16_t)(arr + 1U);
  }

  uint32_t period = (uint32_t)arr + 1U;
  return (uint16_t)(((uint32_t)permille * period) / 1000U);
}
