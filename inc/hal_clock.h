/**
 * @file hal_clock.h
 * @brief Public clock configuration API
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include "hal_types.h"
#include "stm32f103xb.h"

void hal_sysclock_config(void);
void hal_deinit_sysclock(void);

void hal_gpio_init_port_clock(GPIO_TypeDef *port);
void hal_gpio_deinit_port_clock(GPIO_TypeDef *port);

#endif /* HAL_CLOCK_H */
