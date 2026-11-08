/**
 * @file hal_encoder.h
 * @brief Quadrature encoder on a timer: 32-bit position from the 16-bit counter
 *
 * TIM2/3/4 in encoder mode 3: counts every edge of A (CH1) and B (CH2), so
 * 4 counts per encoder line; up when A leads B (RM0008 Table 85). Pins as
 * inputs (pull-up for open-collector encoders): TIM2 PA0/PA1, TIM3 PA6/PA7,
 * TIM4 PB6/PB7.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_ENCODER_H
#define HAL_ENCODER_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

/* caller-owned; call hal_encoder_update before the counter moves 32767
 * counts, or a wrap reads as the wrong direction */
typedef struct {
  TIM_TypeDef *tim;
  uint16_t last;    /* CNT at the previous update */
  int32_t position; /* accumulated counts */
} Encoder;

/* filter: input filter code 0 (none) .. 15 (strongest, RM0008 ICxF) */
HAL_Status hal_encoder_init(Encoder *e, TIM_TypeDef *tim, uint8_t filter,
                            bool invert);
int32_t hal_encoder_update(Encoder *e);
void hal_encoder_set(Encoder *e, int32_t position);

/* signed counts from last to now, taking the shorter way round the wrap */
int32_t hal_encoder_calc_delta(uint16_t now, uint16_t last);

#endif /* HAL_ENCODER_H */
