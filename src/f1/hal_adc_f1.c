/**
 * @file hal_adc_f1.c
 * @brief STM32F1 ADC driver: single conversions and DMA scans, regular group
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_adc.h"
#include "hal_clock.h"
#include "hal_dma.h"
#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define ADC_CH_MAX 17U
#define ADC_SMP_BITS 3U
#define ADC_SMP_MASK 0x7U
#define ADC_CAL_TIMEOUT_MS 10U
#define ADC_CONV_TIMEOUT_MS 2U
#define ADC_SCAN_DRAIN_MS 2U /* > 16 x 252 cycles at 4 MHz */

static HAL_Status adc_wait_cr2_clear(ADC_TypeDef *adc, uint32_t bit) {
  uint32_t start = hal_get_tick();

  while ((adc->CR2 & bit) != 0U) {
    if (hal_tick_expired(hal_get_tick(), start, ADC_CAL_TIMEOUT_MS)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return HAL_OK;
}

HAL_Status hal_adc_init(ADC_TypeDef *adc) {
  HAL_ASSERT((adc == ADC1) || (adc == ADC2));
  if ((adc != ADC1) && (adc != ADC2)) {
    return HAL_ERR_UNSUPPORTED;
  }

  uint8_t pre = 0U;
  HAL_Status st = hal_adc_calc_prescaler(hal_clock_get_pclk2_hz(), &pre);
  if (st != HAL_OK) {
    return st;
  }
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) |
              ((uint32_t)pre << RCC_CFGR_ADCPRE_Pos);
  RCC->APB2ENR |= (adc == ADC1) ? RCC_APB2ENR_ADC1EN : RCC_APB2ENR_ADC2EN;

  /* with ADON set, a CR2 write that changes no other bit starts a conversion
   * (RM0008), so CR2 is fully configured before power-up */
  uint32_t cr2 = ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG; /* start on SWSTART */
  if (adc == ADC1) {
    cr2 |= ADC_CR2_TSVREFE;
  }
  adc->CR2 = cr2;
  adc->CR2 = cr2 | ADC_CR2_ADON;

  /* tSTAB, and >= 2 ADC clocks powered before calibrating (RM0008) */
  uint32_t start = hal_get_tick();
  while (!hal_tick_expired(hal_get_tick(), start, 1U)) {
  }

  adc->CR2 |= ADC_CR2_RSTCAL;
  st = adc_wait_cr2_clear(adc, ADC_CR2_RSTCAL);
  if (st != HAL_OK) {
    return st;
  }
  adc->CR2 |= ADC_CR2_CAL;
  return adc_wait_cr2_clear(adc, ADC_CR2_CAL);
}

void hal_adc_deinit(ADC_TypeDef *adc) {
  adc->CR2 &= ~ADC_CR2_ADON;
  RCC->APB2ENR &= ~((adc == ADC1) ? RCC_APB2ENR_ADC1EN : RCC_APB2ENR_ADC2EN);
}

static void adc_set_sample_time(ADC_TypeDef *adc, uint8_t channel,
                                ADC_SampleTime st) {
  /* SMPR2 holds channels 0-9, SMPR1 channels 10-17, 3 bits each */
  volatile uint32_t *smpr = (channel < 10U) ? &adc->SMPR2 : &adc->SMPR1;
  uint32_t shift = ADC_SMP_BITS * ((channel < 10U) ? channel : channel - 10U);

  *smpr = (*smpr & ~(ADC_SMP_MASK << shift)) | ((uint32_t)st << shift);
}

HAL_Status hal_adc_start(ADC_TypeDef *adc, uint8_t channel,
                         ADC_SampleTime st) {
  HAL_ASSERT(channel <= ADC_CH_MAX);
  if (channel > ADC_CH_MAX) {
    return HAL_ERR_PARAM;
  }

  adc_set_sample_time(adc, channel, st);
  adc->SQR1 = 0U;      /* L = 0: one conversion in the sequence */
  adc->SQR3 = channel; /* SQ1 */
  (void)adc->DR;       /* drop a stale result so try_result waits for this */
  adc->CR2 |= ADC_CR2_SWSTART;
  return HAL_OK;
}

HAL_Status hal_adc_try_result(ADC_TypeDef *adc, uint16_t *value) {
  /* CMSIS names the RM's EOC flag ADC_SR_EOS; reading DR clears it */
  if ((adc->SR & ADC_SR_EOS) == 0U) {
    return HAL_ERR_EMPTY;
  }
  *value = (uint16_t)(adc->DR & 0xFFFU);
  return HAL_OK;
}

HAL_Status hal_adc_read(ADC_TypeDef *adc, uint8_t channel, ADC_SampleTime st,
                        uint16_t *value) {
  HAL_Status res = hal_adc_start(adc, channel, st);
  if (res != HAL_OK) {
    return res;
  }

  uint32_t start = hal_get_tick();
  while ((res = hal_adc_try_result(adc, value)) == HAL_ERR_EMPTY) {
    if (hal_tick_expired(hal_get_tick(), start, ADC_CONV_TIMEOUT_MS)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return res;
}

HAL_Status hal_adc_scan_start(ADC_TypeDef *adc, const uint8_t *channels,
                              uint8_t n, ADC_SampleTime st,
                              volatile uint16_t *buf) {
  static const DMA_Config cfg = {DMA_DIR_PERIPH_TO_MEM, DMA_SIZE_16,
                                 DMA_SIZE_16, false, true, true,
                                 DMA_PRIO_HIGH};
  HAL_ASSERT(adc == ADC1); /* only ADC1 can request DMA (RM0008) */
  HAL_ASSERT((n >= 1U) && (n <= ADC_SCAN_MAX));
  if ((adc != ADC1) || (n == 0U) || (n > ADC_SCAN_MAX)) {
    return HAL_ERR_PARAM;
  }

  for (uint32_t i = 0U; i < n; i++) {
    HAL_ASSERT(channels[i] <= ADC_CH_MAX);
    adc_set_sample_time(adc, channels[i], st);
  }

  uint32_t sqr[3];
  hal_adc_calc_sequence(channels, n, sqr);
  adc->SQR1 = sqr[0];
  adc->SQR2 = sqr[1];
  adc->SQR3 = sqr[2];

  hal_dma_start(DMA1_Channel1, &cfg, &adc->DR, (void *)(uintptr_t)buf, n);

  adc->CR1 |= ADC_CR1_SCAN;
  adc->CR2 |= ADC_CR2_CONT | ADC_CR2_DMA; /* changes bits: not an ADON start */
  adc->CR2 |= ADC_CR2_SWSTART;
  return HAL_OK;
}

void hal_adc_scan_stop(ADC_TypeDef *adc) {
  /* without CONT the round in progress runs to its end, then stops */
  adc->CR2 &= ~ADC_CR2_CONT;
  uint32_t start = hal_get_tick();
  while (!hal_tick_expired(hal_get_tick(), start, ADC_SCAN_DRAIN_MS)) {
  }

  adc->CR2 &= ~ADC_CR2_DMA;
  adc->CR1 &= ~ADC_CR1_SCAN;
  hal_dma_stop(DMA1_Channel1);
  (void)adc->DR; /* clear the last EOC */
}
