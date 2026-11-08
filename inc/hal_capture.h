/**
 * @file hal_capture.h
 * @brief Pulse measurement on a timer: period and high time of one input
 *
 * PWM input mode (RM0008): CH1 latches the period on each rising edge, CH2 the
 * high time on the falling edge, and every rising edge resets the counter, so
 * both are always current with no CPU work. Input on CH1: TIM2 PA0, TIM3 PA6,
 * TIM4 PB6. Measurable period: 1 to 65536 ticks.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_CAPTURE_H
#define HAL_CAPTURE_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdint.h>

typedef struct {
  TIM_TypeDef *tim;
  uint32_t tick_hz;
} Capture;

/* tick_hz must divide the timer clock, e.g. 1 MHz for microsecond ticks */
HAL_Status hal_capture_init(Capture *c, TIM_TypeDef *tim, uint32_t tick_hz);

/* HAL_OK with the latest period and high time in ticks; HAL_ERR_EMPTY if no
 * new period has finished since the last read (signal absent or stuck) */
HAL_Status hal_capture_try_read(Capture *c, uint32_t *period, uint32_t *high);

HAL_Status hal_capture_calc_psc(uint32_t timer_clk_hz, uint32_t tick_hz,
                                uint16_t *psc);

#endif /* HAL_CAPTURE_H */
