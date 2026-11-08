/**
 * @file test_encoder.c
 * @brief Host-side unit tests for the F1 quadrature encoder
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Runs on a TIM_TypeDef in host RAM; hal_timer_base_config is the stand-in
 * from test_pwm.c.
 */

#include "hal_encoder.h"
#include "test_assert.h"

void test_encoder_delta_wraps(void) {
  ASSERT_EQ(hal_encoder_calc_delta(110U, 100U), 10);
  ASSERT_EQ(hal_encoder_calc_delta(90U, 100U), -10);
  ASSERT_EQ(hal_encoder_calc_delta(10U, 65530U), 16);  /* forward over 0 */
  ASSERT_EQ(hal_encoder_calc_delta(65530U, 10U), -16); /* backward over 0 */
  ASSERT_EQ(hal_encoder_calc_delta(32767U, 0U), 32767);
  ASSERT_EQ(hal_encoder_calc_delta(32768U, 0U), -32768); /* ambiguous: back */
}

void test_encoder_init_registers(void) {
  TIM_TypeDef t = {0};
  Encoder e;

  ASSERT_EQ(hal_encoder_init(&e, &t, 6U, false), HAL_OK);
  ASSERT_EQ(t.SMCR & TIM_SMCR_SMS, 3U);       /* encoder mode 3 */
  ASSERT_EQ(t.CCMR1, 0x6161U);                /* CC1S/CC2S = 01, IC1F/IC2F = 6 */
  ASSERT_EQ(t.CCER, 0U);
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, TIM_CR1_CEN);

  ASSERT_EQ(hal_encoder_init(&e, &t, 0U, true), HAL_OK);
  ASSERT_EQ(t.CCER, TIM_CCER_CC1P); /* inverted direction */
}

void test_encoder_update_accumulates(void) {
  TIM_TypeDef t = {0};
  Encoder e;
  (void)hal_encoder_init(&e, &t, 0U, false);

  t.CNT = 100U;
  ASSERT_EQ(hal_encoder_update(&e), 100);
  t.CNT = 65500U; /* 136 counts back, through 0 */
  ASSERT_EQ(hal_encoder_update(&e), -36);
  t.CNT = 40U; /* 76 forward, through 0 again */
  ASSERT_EQ(hal_encoder_update(&e), 40);

  hal_encoder_set(&e, 1000);
  t.CNT = 50U;
  ASSERT_EQ(hal_encoder_update(&e), 1010);
}
