/**
 * @file hal_pwm_calc.c
 * @brief PWM duty arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_pwm.h"
#include <stdint.h>

/* Duty (parts-per-thousand) -> compare value. In PWM mode 1 the output is high
 * while CNT < CCR, and a CCR above ARR holds it high all period (RM0008), so full
 * scale (1000) maps to arr+1. At ARR = 0xFFFF that doesn't fit in 16 bits, so
 * CCR = ARR is the closest: high for all but one count. Values above 1000
 * saturate rather than overflow. */
uint16_t hal_pwm_calc_ccr(uint16_t permille, uint16_t arr) {
  if (permille >= 1000U) {
    if (arr == UINT16_MAX) {
      return arr;
    }
    return (uint16_t)(arr + 1U);
  }
  uint32_t period = (uint32_t)arr + 1U;
  return (uint16_t)(((uint32_t)permille * period) / 1000U);
}
