/**
 * @file hal_i2c_calc.c
 * @brief I2C timing arithmetic
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * CCR sets SCL timing from the APB1 clock. Standard mode (<=100 kHz) is a 1:1
 * duty: CCR = pclk / (2 * scl). Fast mode (400 kHz) uses the 2:1 duty (DUTY=0):
 * CCR = pclk / (3 * scl). TRISE bounds the maximum rise time: it is the number
 * of pclk periods in the mode's max rise time, plus one -- 1000 ns for Sm
 * (FREQ_MHz + 1) and 300 ns for Fm (FREQ_MHz * 300 / 1000 + 1).
 */

#include "hal_i2c.h"
#include <stdint.h>

#define I2C_SCL_STANDARD_HZ 100000U
#define I2C_SCL_FAST_HZ 400000U

uint16_t hal_i2c_calc_ccr(uint32_t pclk_hz, I2C_Speed speed) {
  uint32_t ccr;

  if (speed == I2C_SPEED_FAST) {
    ccr = pclk_hz / (3U * I2C_SCL_FAST_HZ);
  } else {
    ccr = pclk_hz / (2U * I2C_SCL_STANDARD_HZ);
  }

  return (uint16_t)ccr;
}

uint8_t hal_i2c_calc_trise(uint32_t pclk_hz, I2C_Speed speed) {
  uint32_t freq_mhz = pclk_hz / 1000000U;
  uint32_t trise;

  if (speed == I2C_SPEED_FAST) {
    trise = ((freq_mhz * 300U) / 1000U) + 1U;
  } else {
    trise = freq_mhz + 1U;
  }

  return (uint8_t)trise;
}
