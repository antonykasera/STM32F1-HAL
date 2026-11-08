/**
 * @file hal_tim1.h
 * @brief TIM1 advanced PWM: complementary outputs, dead-time, break input
 *
 * For half/full bridges. Pins (no remap): CH1 PA8 / CH1N PB13, CH2 PA9 /
 * CH2N PB14, CH3 PA10 / CH3N PB15, CH4 PA11 (no CH4N), BKIN PB12. Outputs as
 * AF push-pull, BKIN as input. Duty: hal_pwm_set_duty_permille(TIM1, ...).
 *
 * Off means low: whenever outputs are off (stopped, or tripped by the break
 * input) every OCx and OCxN is driven to its idle level, low (OSSI = 1,
 * OISx = 0), so neither switch of a half bridge conducts.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_TIM1_H
#define HAL_TIM1_H

#include "hal_error.h"
#include "hal_pwm.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t freq_hz;
  uint32_t dead_ns;  /* both outputs low this long at each switch, rounded up */
  bool break_enable; /* BKIN low turns every output off at once, in hardware */
} TIM1_Config;

HAL_Status hal_tim1_init(TIM_TypeDef *tim, const TIM1_Config *cfg);
/* PWM mode 1 at 0% duty; complementary also drives CHxN (not on CH4) */
void hal_tim1_channel_init(TIM_TypeDef *tim, PWM_Channel ch,
                           bool complementary);
/* outputs on (MOE) and counter running; also re-arms after a break */
void hal_tim1_start(TIM_TypeDef *tim);
/* outputs to their idle level (low) at once, counter stopped */
void hal_tim1_stop(TIM_TypeDef *tim);
/* a break fired since the last start: outputs stay off until restarted */
bool hal_tim1_tripped(const TIM_TypeDef *tim);

/* DTG code for the shortest dead time >= dead_ns, max 1008 timer ticks */
HAL_Status hal_tim1_calc_dtg(uint32_t timer_clk_hz, uint32_t dead_ns,
                             uint8_t *dtg, uint32_t *actual_ns);
/* BDTR without MOE: DTG, OSSI, OSSR, break active low if enabled */
uint32_t hal_tim1_calc_bdtr(uint8_t dtg, bool break_enable);

#endif /* HAL_TIM1_H */
