/**
 * @file test_pwm.c
 * @brief Host-side unit tests for the F1 PWM duty math
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Invariant: the period spans (arr + 1) counts, so CCR = permille*(arr+1)/1000,
 * with 1000 (100.0%) mapping to arr+1 (output high the whole period) and values
 * above 1000 saturating there.
 */

#include "hal_pwm.h"
#include "hal_timer.h"
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

/* register-level tests on a TIM_TypeDef in host RAM, like test_gpio.c */

/* stand-in: hal_timer_f1.c touches NVIC (ARM-only asm) so can't build on host */
HAL_Status hal_timer_base_config(TIM_TypeDef *tim, uint16_t psc, uint16_t arr) {
  (void)tim;
  (void)psc;
  (void)arr;
  return HAL_OK;
}

#define OCM_PWM1 0x60U  /* OCxM = 110 */
#define OCM_FORCE 0x40U /* OCxM = 100, force inactive */
#define OCPE 0x08U

void test_pwm_channel_init_ch2(void) {
  TIM_TypeDef t = {0};
  t.CCR2 = 0x1234U;

  hal_pwm_channel_init(&t, PWM_CH2);

  ASSERT_EQ((t.CCMR1 >> 8) & 0xFFU, OCM_PWM1 | OCPE); /* CH2 = high byte */
  ASSERT_EQ(t.CCMR1 & 0xFFU, 0U);                     /* CH1 untouched */
  ASSERT_EQ(t.CCER, TIM_CCER_CC2E);
  ASSERT_EQ(t.CCR2, 0U);           /* starts at 0% */
  ASSERT_EQ(t.EGR, TIM_EGR_UG);    /* stopped, so CCR loaded now */
}

void test_pwm_channel_init_ch4_uses_ccmr2(void) {
  TIM_TypeDef t = {0};

  hal_pwm_channel_init(&t, PWM_CH4);

  ASSERT_EQ((t.CCMR2 >> 8) & 0xFFU, OCM_PWM1 | OCPE);
  ASSERT_EQ(t.CCMR1, 0U);
  ASSERT_EQ(t.CCER, TIM_CCER_CC4E);
}

void test_pwm_channel_init_running_no_ug(void) {
  TIM_TypeDef t = {0};
  t.CR1 = TIM_CR1_CEN;

  hal_pwm_channel_init(&t, PWM_CH1);

  ASSERT_EQ(t.EGR, 0U); /* UG would reset the running counter */
}

void test_pwm_stop_forces_low_start_restores(void) {
  TIM_TypeDef t = {0};
  hal_pwm_channel_init(&t, PWM_CH1);
  hal_pwm_channel_init(&t, PWM_CH3);
  t.CR1 = TIM_CR1_CEN;

  hal_pwm_stop(&t);
  ASSERT_EQ(t.CCMR1 & 0xFFU, OCM_FORCE | OCPE); /* preload kept */
  ASSERT_EQ(t.CCMR2 & 0xFFU, OCM_FORCE | OCPE);
  ASSERT_EQ((t.CCMR1 >> 8) & 0xFFU, 0U); /* CH2 not enabled: untouched */
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, 0U);

  hal_pwm_start(&t);
  ASSERT_EQ(t.CCMR1 & 0xFFU, OCM_PWM1 | OCPE);
  ASSERT_EQ(t.CCMR2 & 0xFFU, OCM_PWM1 | OCPE);
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, TIM_CR1_CEN);
}
