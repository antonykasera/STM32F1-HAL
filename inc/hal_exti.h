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

void hal_exti_init(GPIO_TypeDef *port);

#endif /* HAL_EXTI_H */
