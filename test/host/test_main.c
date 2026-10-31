/**
 * @file test_main.c
 * @brief Host-side test runner: owns the shared counters, summary, and main().
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Each module's tests live in test_<mod>.c as `void test_xxx(void)`. Declare
 * them here and add a TEST_RUN line in main().
 */

#include "test_assert.h"

unsigned tests_run = 0;
unsigned tests_failed = 0;
unsigned checks_failed = 0;

int test_summary(void) {
  printf("\n%u tests, %u failed, %u checks failed\n", tests_run, tests_failed,
         checks_failed);
  return tests_failed == 0 ? 0 : 1;
}

/* --- gpio (test_gpio.c) --- */
void test_encode_output_modes(void);
void test_encode_input_modes(void);
void test_cr_shift(void);
void test_pin_init_pc13(void);
void test_pin_init_crl_isolation(void);
void test_pin_init_pull_direction(void);
void test_write_and_toggle(void);

/* --- clock (test_clock.c) --- */
void test_flash_latency_boundaries(void);
void test_sysclk_from_config(void);
void test_default_config_picks_2ws(void);
void test_tick_expired(void);

/* --- timer (test_timer.c) --- */
void test_solver_no_prescale(void);
void test_solver_1khz_needs_prescale(void);
void test_solver_needs_prescale(void);
void test_solver_1khz_from_36mhz(void);
void test_solver_guards_bad_input(void);

/* --- uart (test_uart.c) --- */
void test_brr_72mhz_115200(void);
void test_brr_72mhz_9600(void);
void test_brr_36mhz_115200(void);
void test_brr_zero_baud_guarded(void);

/* --- pwm (test_pwm.c) --- */
void test_pwm_duty_zero(void);
void test_pwm_duty_full(void);
void test_pwm_duty_half(void);
void test_pwm_duty_quarter(void);
void test_pwm_duty_clamps_over_1000(void);
void test_pwm_duty_rounds_down(void);

/* --- i2c (test_i2c.c) --- */
void test_i2c_ccr_standard(void);
void test_i2c_ccr_fast(void);
void test_i2c_trise_standard(void);
void test_i2c_trise_fast(void);

/* --- error (test_error.c) --- */
void test_status_str_known(void);
void test_status_str_unknown(void);
void test_assert_true_is_noop(void);

int main(void) {
  TEST_RUN(test_encode_output_modes);
  TEST_RUN(test_encode_input_modes);
  TEST_RUN(test_cr_shift);
  TEST_RUN(test_pin_init_pc13);
  TEST_RUN(test_pin_init_crl_isolation);
  TEST_RUN(test_pin_init_pull_direction);
  TEST_RUN(test_write_and_toggle);

  TEST_RUN(test_flash_latency_boundaries);
  TEST_RUN(test_sysclk_from_config);
  TEST_RUN(test_default_config_picks_2ws);
  TEST_RUN(test_tick_expired);

  TEST_RUN(test_solver_no_prescale);
  TEST_RUN(test_solver_1khz_needs_prescale);
  TEST_RUN(test_solver_needs_prescale);
  TEST_RUN(test_solver_1khz_from_36mhz);
  TEST_RUN(test_solver_guards_bad_input);

  TEST_RUN(test_brr_72mhz_115200);
  TEST_RUN(test_brr_72mhz_9600);
  TEST_RUN(test_brr_36mhz_115200);
  TEST_RUN(test_brr_zero_baud_guarded);

  TEST_RUN(test_pwm_duty_zero);
  TEST_RUN(test_pwm_duty_full);
  TEST_RUN(test_pwm_duty_half);
  TEST_RUN(test_pwm_duty_quarter);
  TEST_RUN(test_pwm_duty_clamps_over_1000);
  TEST_RUN(test_pwm_duty_rounds_down);

  TEST_RUN(test_i2c_ccr_standard);
  TEST_RUN(test_i2c_ccr_fast);
  TEST_RUN(test_i2c_trise_standard);
  TEST_RUN(test_i2c_trise_fast);

  TEST_RUN(test_status_str_known);
  TEST_RUN(test_status_str_unknown);
  TEST_RUN(test_assert_true_is_noop);

  return TEST_SUMMARY();
}
