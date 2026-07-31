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
