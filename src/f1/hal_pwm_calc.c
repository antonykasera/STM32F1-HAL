/**
 * @file hal_pwm_calc.c
 * @brief PWM duty arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_pwm.h"
#include <stdint.h>

/* permille -> CCR. CCR > ARR holds the output high (RM0008); at ARR = 0xFFFF
 * nothing is larger, so 100% is CCR = ARR (high for all but one count). */
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
