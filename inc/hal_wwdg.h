/**
 * @file hal_wwdg.h
 * @brief Window watchdog: resets if refreshed too late OR too early
 *
 * Counts PCLK1 / 4096 / 2^WDGTB from the reload down to 0x40 and resets on
 * 0x3F: 113 us to 58 ms at 36 MHz. The first tick after a refresh can land
 * anywhere within one tick, so both limits are good to one tick. Once started
 * it only stops on reset.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_WWDG_H
#define HAL_WWDG_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

/* min_us: no refresh before this; max_us: reset unless refreshed by then.
 * min_us 0 = no window. Smallest prescaler that fits, for the finest tick. */
HAL_Status hal_wwdg_calc(uint32_t pclk1_hz, uint32_t min_us, uint32_t max_us,
                         uint32_t *cfr, uint8_t *cr);

/* needs hal_sysclock_init() first: the tick comes from PCLK1 */
HAL_Status hal_wwdg_start(WWDG_TypeDef *wwdg, uint32_t min_us, uint32_t max_us);
void hal_wwdg_refresh(WWDG_TypeDef *wwdg);

/* pause the countdown while a debugger has the core halted */
void hal_wwdg_freeze_in_debug(void);

/* true if the last reset came from the WWDG; clears all RCC reset flags, as
 * hal_iwdg_caused_reset() does, so ask only one of them per boot */
bool hal_wwdg_caused_reset(void);

#endif /* HAL_WWDG_H */
