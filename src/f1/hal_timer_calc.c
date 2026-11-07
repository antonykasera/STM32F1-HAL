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

/* Split ticks = timer_clk / freq into (PSC+1) * (ARR+1), keeping PSC as small as
 * possible so ARR, and with it duty resolution, stays as large as possible.
 *
 * ARR+1 = ticks / (p+1) must be <= 65536. The smallest p for which that holds
 * is ticks / 65537 (integer division), so no search is needed. Since
 * ticks < 2^32 = 65537 * 65536 + 1, p never exceeds 65535 and always fits PSC.
 *
 * ticks < 2 has no valid ARR: ticks = 1 gives ARR = 0, which blocks the counter
 * (RM0008), and ticks = 0 would wrap ARR to 0xFFFF. */
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
