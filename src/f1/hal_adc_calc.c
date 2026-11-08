/**
 * @file hal_adc_calc.c
 * @brief ADC clock and conversion arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Sensor figures from the STM32F103x8 datasheet: VREFINT 1.20 V, V25 1.43 V,
 * Avg_Slope 4.3 mV/C (typical).
 */

#include "hal_adc.h"
#include <stddef.h>
#include <stdint.h>

#define ADC_CLK_MAX_HZ 14000000U
#define ADC_FULL_SCALE 4095U
#define ADC_VREFINT_MV 1200U
#define ADC_V25_MV 1430U

HAL_Status hal_adc_calc_prescaler(uint32_t pclk2_hz, uint8_t *code) {
  static const uint8_t divs[] = {2U, 4U, 6U, 8U}; /* ADCPRE 00..11 */

  if (code == NULL) {
    return HAL_ERR_PARAM;
  }
  for (uint8_t i = 0U; i < 4U; i++) {
    if ((pclk2_hz / divs[i]) <= ADC_CLK_MAX_HZ) {
      *code = i;
      return HAL_OK;
    }
  }
  return HAL_ERR_PARAM;
}

/* 5 bits per slot: SQ1-6 in SQR3, SQ7-12 in SQR2, SQ13-16 and L in SQR1 */
void hal_adc_calc_sequence(const uint8_t *channels, uint8_t n,
                           uint32_t sqr[3]) {
  sqr[0] = (uint32_t)(n - 1U) << ADC_SQR1_L_Pos;
  sqr[1] = 0U;
  sqr[2] = 0U;

  for (uint32_t i = 0U; i < n; i++) {
    uint32_t reg = 2U - (i / 6U);
    uint32_t shift = 5U * (i % 6U);
    sqr[reg] |= ((uint32_t)channels[i] & 0x1FU) << shift;
  }
}

uint32_t hal_adc_calc_vdda_mv(uint16_t vrefint_raw) {
  if (vrefint_raw == 0U) {
    return 0U;
  }
  return ((ADC_VREFINT_MV * ADC_FULL_SCALE) + (vrefint_raw / 2U)) / vrefint_raw;
}

uint32_t hal_adc_calc_mv(uint16_t raw, uint32_t vdda_mv) {
  return (((uint32_t)raw * vdda_mv) + (ADC_FULL_SCALE / 2U)) / ADC_FULL_SCALE;
}

int32_t hal_adc_calc_temp_c10(uint32_t sense_mv) {
  /* T = (V25 - V) / 4.3 mV + 25 C, in tenths: (V25 - V) * 100 / 43 + 250 */
  int32_t delta_mv = (int32_t)ADC_V25_MV - (int32_t)sense_mv;
  return ((delta_mv * 100) / 43) + 250;
}
