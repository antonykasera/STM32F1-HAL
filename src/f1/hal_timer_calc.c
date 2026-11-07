/**
 * @file hal_timer_calc.c
 * @brief Timer arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_timer.h"
#include <stddef.h>
#include <stdint.h>

/* smallest PSC keeping ticks/(PSC+1) <= 65536, i.e. max ARR (duty resolution).
 * ticks < 2 has no valid ARR: ARR = 0 blocks the counter (RM0008). */
HAL_Status hal_timer_solve_psc_arr(uint32_t timer_clk_hz, uint32_t freq_hz,
                                   uint16_t *psc, uint16_t *arr) {
  if ((freq_hz == 0U) || (psc == NULL) || (arr == NULL)) {
    return HAL_ERR_PARAM;
  }

  uint32_t ticks = timer_clk_hz / freq_hz;
  if (ticks < 2U) {
    return HAL_ERR_PARAM;
  }

  uint32_t p = ticks / 65537U;

  *psc = (uint16_t)p;
  *arr = (uint16_t)((ticks / (p + 1U)) - 1U);
  return HAL_OK;
}
