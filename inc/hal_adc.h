/**
 * @file hal_adc.h
 * @brief 12-bit ADC: single conversions, blocking or polled
 *
 * Channels 0-7 = PA0-PA7, 8-9 = PB0-PB1 (pins as GPIO_MODE_ANALOG), 16 = temp
 * sensor, 17 = VREFINT (ADC1 only).
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define ADC_CH_TEMP 16U
#define ADC_CH_VREFINT 17U

/* sampling time in ADC clock cycles; temp/VREFINT need >= 17.1 us (239.5) */
typedef enum {
  ADC_SAMPLE_1_5 = 0,
  ADC_SAMPLE_7_5,
  ADC_SAMPLE_13_5,
  ADC_SAMPLE_28_5,
  ADC_SAMPLE_41_5,
  ADC_SAMPLE_55_5,
  ADC_SAMPLE_71_5,
  ADC_SAMPLE_239_5,
} ADC_SampleTime;

/* power up and calibrate; needs hal_systick_init(). ADC1 also enables the
 * temp sensor and VREFINT channels. */
HAL_Status hal_adc_init(ADC_TypeDef *adc);
void hal_adc_deinit(ADC_TypeDef *adc);

/* non-blocking: start, then poll try_result (HAL_ERR_EMPTY until done) */
HAL_Status hal_adc_start(ADC_TypeDef *adc, uint8_t channel, ADC_SampleTime st);
HAL_Status hal_adc_try_result(ADC_TypeDef *adc, uint16_t *value);

/* blocking single conversion (< 0.5 ms at any legal ADC clock) */
HAL_Status hal_adc_read(ADC_TypeDef *adc, uint8_t channel, ADC_SampleTime st,
                        uint16_t *value);

#define ADC_SCAN_MAX 16U

/* ADC1 only: convert channels[0..n-1] over and over while DMA ch1 copies each
 * result into buf[i]. buf updates in the background and must outlive the
 * scan; a read may mix two rounds. No single reads until scan_stop. */
HAL_Status hal_adc_scan_start(ADC_TypeDef *adc, const uint8_t *channels,
                              uint8_t n, ADC_SampleTime st,
                              volatile uint16_t *buf);
/* lets the round in progress finish (~2 ms) so single reads start clean */
void hal_adc_scan_stop(ADC_TypeDef *adc);

/* SQR1, SQR2, SQR3 values for a regular sequence of n (1..16) channels */
void hal_adc_calc_sequence(const uint8_t *channels, uint8_t n,
                           uint32_t sqr[3]);

/* ADCPRE code for the fastest ADC clock <= 14 MHz from pclk2 */
HAL_Status hal_adc_calc_prescaler(uint32_t pclk2_hz, uint8_t *code);

/* VDDA from a VREFINT reading (1.20 V typical, 1.16-1.24 V spread) */
uint32_t hal_adc_calc_vdda_mv(uint16_t vrefint_raw);
uint32_t hal_adc_calc_mv(uint16_t raw, uint32_t vdda_mv);
/* temp sensor in 0.1 C from its voltage; V25 spreads 1.34-1.52 V between
 * chips, so trust changes more than the absolute value */
int32_t hal_adc_calc_temp_c10(uint32_t sense_mv);

#endif /* HAL_ADC_H */
