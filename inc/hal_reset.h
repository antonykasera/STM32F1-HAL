/**
 * @file hal_reset.h
 * @brief Reset cause and software reset
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_RESET_H
#define HAL_RESET_H

#include <stdint.h>

/* bits of the cause mask, in RCC_CSR order. More than one can be set (RM0008:
 * a power reset leaves POR | PIN; the bench clone sets only the cause's own
 * flag), so test the bit you care about. Exiting Standby is a power reset:
 * hal_pwr_woke_from_standby tells it from power-on */
typedef enum {
  HAL_RESET_PIN = 1U << 0,
  HAL_RESET_POWER_ON = 1U << 1,
  HAL_RESET_SOFTWARE = 1U << 2,
  HAL_RESET_IWDG = 1U << 3,
  HAL_RESET_WWDG = 1U << 4,
  HAL_RESET_LOW_POWER = 1U << 5
} HAL_Reset_Cause;

uint32_t hal_reset_calc_cause(uint32_t csr);

/* the first call reads and clears the RCC flags; later calls return the same
 * mask, so any number of modules can ask during one boot */
uint32_t hal_reset_cause(void);

/* the most specific cause in the mask, e.g. "iwdg" for IWDG | PIN */
const char *hal_reset_cause_str(uint32_t cause);

_Noreturn void hal_reset_software(void);

#endif /* HAL_RESET_H */
