/**
 * @file test_adc.c
 * @brief Host-side unit tests for the F1 ADC math and conversion sequence
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * VREFINT 1.20 V, V25 1.43 V, 4.3 mV/C: datasheet typicals.
 */

#include "hal_adc.h"
#include "test_assert.h"

static uint8_t pre(uint32_t pclk2) {
  uint8_t code = 0xFFU;
  ASSERT_EQ(hal_adc_calc_prescaler(pclk2, &code), HAL_OK);
  return code;
}

void test_adc_prescaler(void) {
  ASSERT_EQ(pre(72000000U), 2U); /* /6 = 12 MHz (/4 = 18 is too fast) */
  ASSERT_EQ(pre(36000000U), 1U); /* /4 = 9 MHz */
  ASSERT_EQ(pre(28000000U), 0U); /* /2 = 14 MHz exactly */
  ASSERT_EQ(pre(8000000U), 0U);  /* HSI: /2 = 4 MHz */
}

void test_adc_vdda_and_mv(void) {
  /* VREFINT reads 1.20/3.30 * 4095 = 1489 at VDDA 3.3 V */
  ASSERT_EQ(hal_adc_calc_vdda_mv(1489U), 3300U);
  ASSERT_EQ(hal_adc_calc_vdda_mv(1638U), 3000U);
  ASSERT_EQ(hal_adc_calc_vdda_mv(0U), 0U); /* guarded */

  ASSERT_EQ(hal_adc_calc_mv(4095U, 3300U), 3300U);
  ASSERT_EQ(hal_adc_calc_mv(2048U, 3300U), 1650U);
  ASSERT_EQ(hal_adc_calc_mv(0U, 3300U), 0U);
}

void test_adc_temp(void) {
  ASSERT_EQ(hal_adc_calc_temp_c10(1430U), 250); /* V25 */
  ASSERT_EQ(hal_adc_calc_temp_c10(1387U), 350); /* 43 mV lower = +10 C */
  ASSERT_EQ(hal_adc_calc_temp_c10(1473U), 150); /* slope is negative */
}

/* register-level, on an ADC_TypeDef in host RAM */

void test_adc_start_sets_sequence(void) {
  ADC_TypeDef a = {0};

  ASSERT_EQ(hal_adc_start(&a, 5U, ADC_SAMPLE_55_5), HAL_OK);
  ASSERT_EQ(a.SMPR2, 5U << (3U * 5U)); /* ch 5 in SMPR2 */
  ASSERT_EQ(a.SQR3, 5U);                /* SQ1 = channel 5 */
  ASSERT_EQ(a.CR2 & ADC_CR2_SWSTART, ADC_CR2_SWSTART);

  ASSERT_EQ(hal_adc_start(&a, ADC_CH_TEMP, ADC_SAMPLE_239_5), HAL_OK);
  ASSERT_EQ(a.SMPR1, 7U << (3U * 6U)); /* ch 16 in SMPR1 */
  ASSERT_EQ(a.SQR3, 16U);
}

void test_adc_try_result(void) {
  ADC_TypeDef a = {0};
  uint16_t v = 0xAAAAU;

  ASSERT_EQ(hal_adc_try_result(&a, &v), HAL_ERR_EMPTY);
  ASSERT_EQ(v, 0xAAAAU);

  a.SR = ADC_SR_EOS; /* the RM's EOC */
  a.DR = 0x0ABCU;
  ASSERT_EQ(hal_adc_try_result(&a, &v), HAL_OK);
  ASSERT_EQ(v, 0x0ABCU);
}

void test_adc_sequence_short(void) {
  const uint8_t ch[] = {0U, 1U, ADC_CH_VREFINT, ADC_CH_TEMP};
  uint32_t sqr[3];

  hal_adc_calc_sequence(ch, 4U, sqr);

  ASSERT_EQ(sqr[0], 0x00300000U); /* SQR1: L = 3 (4 conversions) */
  ASSERT_EQ(sqr[1], 0U);          /* SQR2 unused */
  ASSERT_EQ(sqr[2], 0x00084420U); /* SQR3: 0, 1, 17, 16 */
}

void test_adc_sequence_full(void) {
  uint8_t ch[16];
  uint32_t sqr[3];
  for (uint8_t i = 0U; i < 16U; i++) {
    ch[i] = i;
  }

  hal_adc_calc_sequence(ch, 16U, sqr);

  ASSERT_EQ(sqr[0], 0x00F7B9ACU); /* SQ13-16 = 12..15, L = 15 */
  ASSERT_EQ(sqr[1], 0x16A4A0E6U); /* SQ7-12 = 6..11 */
  ASSERT_EQ(sqr[2], 0x0A418820U); /* SQ1-6 = 0..5 */
}
