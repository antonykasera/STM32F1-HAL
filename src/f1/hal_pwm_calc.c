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
