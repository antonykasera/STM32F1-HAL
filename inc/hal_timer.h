/**
 * @file hal_timer.h
 * @brief TIMER Configurations
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "hal_types.h"
#include "stm32f103xb.h"
#include <stdint.h>

typedef void (*timer_callback_t)(void);

void hal_timer_solve_psc_arr(uint32_t timer_clk_hz, uint32_t freq_hz,
                             uint16_t *psc, uint16_t *arr);

void hal_timer_init(TIM_TypeDef *tim, uint16_t psc, uint16_t arr);

void hal_timer_init_hz(TIM_TypeDef *tim, uint32_t freq_hz);

void hal_timer_deinit(TIM_TypeDef *tim);

void hal_timer_start(TIM_TypeDef *tim);

void hal_timer_stop(TIM_TypeDef *tim);

void hal_timer_register_callback(TIM_TypeDef *tim, timer_callback_t cb);

void hal_timer_set_priority(TIM_TypeDef *tim, uint8_t priority);

#endif /* HAL_TIMER_H */
