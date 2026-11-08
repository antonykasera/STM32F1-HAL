/**
 * @file hal_wwdg_f1.c
 * @brief STM32F1 window watchdog driver
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_wwdg.h"
#include "hal_reset.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

/* WDGA | T, written on every refresh; T6 must stay set or it resets at once */
static uint8_t wwdg_reload;

HAL_Status hal_wwdg_start(WWDG_TypeDef *wwdg, uint32_t min_us,
                          uint32_t max_us) {
  uint32_t cfr = 0U;
  uint8_t cr = 0U;
  HAL_Status st =
      hal_wwdg_calc(hal_clock_get_pclk1_hz(), min_us, max_us, &cfr, &cr);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return st;
  }

  RCC->APB1ENR |= RCC_APB1ENR_WWDGEN;
  wwdg->CFR = cfr; /* window first: WDGA arms it */
  wwdg_reload = cr;
  wwdg->CR = cr;
  return HAL_OK;
}

void hal_wwdg_refresh(WWDG_TypeDef *wwdg) { wwdg->CR = wwdg_reload; }

void hal_wwdg_freeze_in_debug(void) {
  DBGMCU->CR |= DBGMCU_CR_DBG_WWDG_STOP;
}

bool hal_wwdg_caused_reset(void) {
  return (hal_reset_cause() & HAL_RESET_WWDG) != 0U;
}
