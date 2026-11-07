/**
 * @file hal_pwm.h
 * @brief STM32F1 PWM Configuration
 *
 * Edge-aligned PWM (mode 1) on the general-purpose timers TIM2/3/4, all four
 * capture/compare channels. The timer counts 0..ARR (setting the frequency);
 * the output is high while CNT < CCR, so CCR sets the duty cycle. Output is
 * driven by timer hardware (no CPU/ISR per edge) — configure the pin as
 * alternate-function push-pull. Advanced-timer features (TIM1 break/dead-time)
 * are out of scope.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_PWM_H
#define HAL_PWM_H

#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { PWM_CH1 = 0, PWM_CH2, PWM_CH3, PWM_CH4 } PWM_Channel;

uint16_t hal_pwm_calc_ccr(uint16_t permille, uint16_t arr);

void hal_pwm_init(TIM_TypeDef *tim, PWM_Channel ch, uint32_t freq_hz);

void hal_pwm_set_duty_permille(TIM_TypeDef *tim, PWM_Channel ch,
                               uint16_t permille);

void hal_pwm_set_duty_raw(TIM_TypeDef *tim, PWM_Channel ch, uint16_t ccr);

/* stop drives all enabled outputs low before halting; start resumes PWM */
void hal_pwm_start(TIM_TypeDef *tim);
void hal_pwm_stop(TIM_TypeDef *tim);

void hal_pwm_channel_enable(TIM_TypeDef *tim, PWM_Channel ch, bool en);

#endif /* HAL_PWM_H */
