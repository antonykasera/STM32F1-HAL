/**
 * @file test_i2c.c
 * @brief Host-side unit tests for the F1 I2C timing math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Only hal_i2c_calc_ccr / hal_i2c_calc_trise are exercised
 * (APB1 clock + speed mode -> CCR / TRISE), no register access. The state
 * machine (init, transactions) touches I2C/RCC and is NOT host-callable.
 *
 * At pclk1 = 36 MHz:
 *   Sm (100 kHz): CCR = 36e6/(2*100e3) = 180, TRISE = 36 + 1 = 37
 *   Fm (400 kHz): CCR = 36e6/(3*400e3) = 30,  TRISE = 36*300/1000 + 1 = 11
 */

#include "hal_i2c.h"
#include "test_assert.h"

#define PCLK1 36000000U

void test_i2c_ccr_standard(void) {
  ASSERT_EQ(hal_i2c_calc_ccr(PCLK1, I2C_SPEED_STANDARD), 180U);
}

void test_i2c_ccr_fast(void) {
  ASSERT_EQ(hal_i2c_calc_ccr(PCLK1, I2C_SPEED_FAST), 30U);
}

void test_i2c_trise_standard(void) {
  ASSERT_EQ(hal_i2c_calc_trise(PCLK1, I2C_SPEED_STANDARD), 37U);
}

void test_i2c_trise_fast(void) {
  ASSERT_EQ(hal_i2c_calc_trise(PCLK1, I2C_SPEED_FAST), 11U);
}
