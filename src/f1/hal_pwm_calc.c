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
