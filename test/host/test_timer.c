/**
 * @file test_timer.c
 * @brief Host-side unit tests for the F1 timer math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Only hal_timer_solve_psc_arr is exercised -- it is pure arithmetic
 * (timer clock + target frequency -> PSC/ARR), no register access. The register
 * functions (hal_timer_init, start/stop, handlers) touch TIM/RCC/NVIC and are
 * NOT host-callable.
 *
 * Invariant checked: update_freq = timer_clk / ((PSC+1) * (ARR+1)), with ARR
 * fitting in 16 bits (<= 65535) and the solver maximising ARR (smallest PSC).
 */

#include "hal_timer.h"
#include "stm32f103xb.h"
#include "test_assert.h"

#define TCLK 72000000U

/* assert the solved pair reproduces the requested frequency exactly, and that
 * ARR stayed within the 16-bit register width. */
static void check_solves_to(uint32_t freq, uint32_t expect_psc,
                            uint32_t expect_arr) {
  uint16_t psc = 0xFFFF;
  uint16_t arr = 0xFFFF;

  hal_timer_solve_psc_arr(TCLK, freq, &psc, &arr);

  ASSERT_EQ(psc, expect_psc);
  ASSERT_EQ(arr, expect_arr);

  uint32_t produced = TCLK / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U));
  ASSERT_EQ(produced, freq);
}

void test_solver_no_prescale(void) {
  /* 72 MHz / 10000 Hz = 7200 ticks, fits in ARR alone: PSC=0, ARR=7199. */
  check_solves_to(10000U, 0U, 7199U);
}

void test_solver_1khz_needs_prescale(void) {
  /* 72 MHz / 1000 Hz = 72000 ticks. 72000 > 65536 so ARR alone overflows;
   * solver engages PSC=1 -> 72000/2 = 36000 -> ARR=35999. */
  check_solves_to(1000U, 1U, 35999U);
}

void test_solver_needs_prescale(void) {
  /* 1 Hz -> 72,000,000 ticks. Too big for ARR alone (>65536), so PSC grows.
   * ticks/(psc+1) must be <= 65536; smallest psc is 1099 -> 72e6/1100 = 65454.
   */
  uint16_t psc = 0, arr = 0;
  hal_timer_solve_psc_arr(TCLK, 1U, &psc, &arr);

  /* arr is uint16_t so it cannot exceed 65535 by construction; assert the
   * solver actually engaged the prescaler (psc > 0) and hit the target. */
  ASSERT_TRUE(psc > 0U);
  uint32_t produced = TCLK / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U));
  ASSERT_EQ(produced, 1U);
}

void test_solver_1khz_from_36mhz(void) {
  /* if the caller passed a 36 MHz timer clock (APB1 x1 case) instead of 72:
   * 36e6/1000 = 36000 ticks, fits alone: PSC=0, ARR=35999. */
  uint16_t psc = 0xFFFF, arr = 0xFFFF;
  hal_timer_solve_psc_arr(36000000U, 1000U, &psc, &arr);
  ASSERT_EQ(psc, 0U);
  ASSERT_EQ(arr, 35999U);
}

void test_solver_guards_bad_input(void) {
  uint16_t psc = 0x1234, arr = 0x5678;

  /* freq 0 must be ignored -- outputs untouched. */
  hal_timer_solve_psc_arr(TCLK, 0U, &psc, &arr);
  ASSERT_EQ(psc, 0x1234U);
  ASSERT_EQ(arr, 0x5678U);

  /* NULL pointers must not crash. */
  hal_timer_solve_psc_arr(TCLK, 1000U, NULL, NULL);
}
