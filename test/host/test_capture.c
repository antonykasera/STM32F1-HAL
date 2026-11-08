/**
 * @file test_capture.c
 * @brief Host-side unit tests for F1 pulse measurement
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Register tests run on a TIM_TypeDef in host RAM with test_pwm.c's
 * hal_timer_base_config stand-in. On the host every timer clock is the 8 MHz
 * HSI default.
 */

#include "hal_capture.h"
#include "test_assert.h"

void test_capture_psc(void) {
  uint16_t psc = 0U;

  ASSERT_EQ(hal_capture_calc_psc(72000000U, 1000000U, &psc), HAL_OK);
  ASSERT_EQ(psc, 71U); /* 1 us ticks */
  ASSERT_EQ(hal_capture_calc_psc(72000000U, 72000000U, &psc), HAL_OK);
  ASSERT_EQ(psc, 0U);
  ASSERT_EQ(hal_capture_calc_psc(72000000U, 1125U, &psc), HAL_OK);
  ASSERT_EQ(psc, 63999U); /* 72e6 / 1125 = 64000, near the 65536 limit */

  psc = 0x1234U;
  ASSERT_EQ(hal_capture_calc_psc(72000000U, 7000000U, &psc), HAL_ERR_PARAM);
  ASSERT_EQ(hal_capture_calc_psc(72000000U, 1000U, &psc), HAL_ERR_PARAM);
  ASSERT_EQ(hal_capture_calc_psc(72000000U, 0U, &psc), HAL_ERR_PARAM);
  ASSERT_EQ(psc, 0x1234U); /* untouched on error */
}

void test_capture_init_registers(void) {
  TIM_TypeDef t = {0};
  Capture c;

  ASSERT_EQ(hal_capture_init(&c, &t, 1000000U), HAL_OK);
  ASSERT_EQ(t.CCMR1, 0x0201U);   /* CC1S = 01, CC2S = 10 */
  ASSERT_EQ(t.CCER, 0x0031U);    /* CC1E, CC2E, CC2P */
  ASSERT_EQ(t.SMCR, 0x0054U);    /* TS = 101, SMS = 100 */
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, TIM_CR1_CEN);
}

void test_capture_try_read(void) {
  TIM_TypeDef t = {0};
  Capture c = {&t, 1000000U};
  uint32_t period = 7U;
  uint32_t high = 7U;

  ASSERT_EQ(hal_capture_try_read(&c, &period, &high), HAL_ERR_EMPTY);
  ASSERT_EQ(period, 7U);

  t.SR = TIM_SR_CC1IF;
  t.CCR1 = 999U;
  t.CCR2 = 249U;
  ASSERT_EQ(hal_capture_try_read(&c, &period, &high), HAL_OK);
  ASSERT_EQ(period, 1000U);
  ASSERT_EQ(high, 250U);
}
