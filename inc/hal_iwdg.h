/**
 * @file hal_iwdg.h
 * @brief Independent watchdog: resets the chip unless fed in time
 *
 * Clocked by the LSI, 30-60 kHz (40 typical, datasheet), so a requested timeout
 * really lands between 0.67x and 1.33x. Once started it only stops on reset.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_IWDG_H
#define HAL_IWDG_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define IWDG_LSI_HZ 40000U /* nominal; timeouts are computed from this */

/* smallest prescaler that fits, for the finest reload step; 1..~26200 ms */
HAL_Status hal_iwdg_calc(uint32_t timeout_ms, uint8_t *pr, uint16_t *rlr);

/* needs hal_systick_init() for the register-update timeout */
HAL_Status hal_iwdg_start(IWDG_TypeDef *iwdg, uint32_t timeout_ms);
void hal_iwdg_feed(IWDG_TypeDef *iwdg);

/* pause the countdown while a debugger has the core halted */
void hal_iwdg_freeze_in_debug(void);

/* true if the last reset came from the IWDG (hal_reset_cause) */
bool hal_iwdg_caused_reset(void);

#endif /* HAL_IWDG_H */
