/**
 * @file hal_exti.h
 * @brief Public EXTI/interrupt API
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include "hal_types.h"
#include "stm32f103xb.h"
#include <stdint.h>

typedef enum { FALLING_EDGE, RISING_EDGE } EXTI_Edge_t;

typedef void (*exti_callback_t)(void);

void hal_exti_init(GPIO_TypeDef *port, uint8_t pin, EXTI_Edge_t edge);

void hal_exti_register_callback(uint8_t line, exti_callback_t cb);

#endif /* HAL_EXTI_H */
