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
#include <stdint.h>

void hal_sysclock_init(void);
void hal_sysclock_deinit(void);

void hal_systick_init(void);
uint32_t hal_get_tick(void);
void hal_delay_ms(uint32_t delay_ms);

void hal_gpio_init_port_clock(GPIO_TypeDef *port);
void hal_gpio_deinit_port_clock(GPIO_TypeDef *port);

#endif /* HAL_CLOCK_H */
