/**
 * @file test_pwm.c
 * @brief Host-side unit tests for the F1 PWM duty math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Invariant: the period spans (arr + 1) counts, so CCR = permille*(arr+1)/1000,
 * with 1000 (100.0%) mapping to arr+1 (output high the whole period) and values
 * above 1000 saturating there.
 */

#include "hal_pwm.h"
#include "test_assert.h"
#include <stdint.h>

void test_pwm_duty_zero(void) {
  /* 0% -> CCR 0 (output never high). */
  ASSERT_EQ(hal_pwm_calc_ccr(0U, 999U), 0U);
}

void test_pwm_duty_full(void) {
  /* 100.0% -> CCR = arr+1 (>= period, so CNT < CCR always -> always high). */
  ASSERT_EQ(hal_pwm_calc_ccr(1000U, 999U), 1000U);
}

void test_pwm_duty_half(void) {
  /* 50.0% of a 1000-count period (arr=999) -> CCR 500. */
  ASSERT_EQ(hal_pwm_calc_ccr(500U, 999U), 500U);
}

void test_pwm_duty_quarter(void) {
  /* 25.0% -> 250 (the pwm_led example value). */
  ASSERT_EQ(hal_pwm_calc_ccr(250U, 999U), 250U);
}

void test_pwm_duty_clamps_over_1000(void) {
  /* anything past full scale saturates at arr+1, never overflows. */
  ASSERT_EQ(hal_pwm_calc_ccr(5000U, 999U), 1000U);
}

void test_pwm_duty_rounds_down(void) {
  /* integer division truncates: 333 permille of 100 counts = 33.3 -> 33. */
  ASSERT_EQ(hal_pwm_calc_ccr(333U, 99U), 33U);
}

void test_pwm_duty_full_at_max_arr(void) {
  /* ARR = 0xFFFF, arr +1 doesn't fit in 16 bits.
   * closest 16-bit CCR can get, CCR = ARR, high for
   * all but one count of the period */
  ASSERT_EQ(hal_pwm_calc_ccr(1000U, 0xFFFFU), 0xFFFFU);
  ASSERT_EQ(hal_pwm_calc_ccr(5000U, 0xFFFFU), 0xFFFFU);

  ASSERT_EQ(hal_pwm_calc_ccr(999U, 0xFFFFU), 65470U);
}
