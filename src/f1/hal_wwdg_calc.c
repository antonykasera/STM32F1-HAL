/**
 * @file hal_wwdg_calc.c
 * @brief WWDG timeout and window arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * tick = 4096 * 2^WDGTB / PCLK1; timeout = (T[5:0] + 1) ticks (RM0008).
 * A refresh resets the chip while the counter is still above W.
 */

#include "hal_wwdg.h"
#include <stddef.h>
#include <stdint.h>

#define WWDG_TB_MAX 3U
#define WWDG_TICKS_MAX 64U  /* T[5:0] + 1 */
#define WWDG_T_FLOOR 0x3FU  /* reset when the counter reaches this */

HAL_Status hal_wwdg_calc(uint32_t pclk1_hz, uint32_t min_us, uint32_t max_us,
                         uint32_t *cfr, uint8_t *cr) {
  if ((pclk1_hz == 0U) || (min_us >= max_us) || (cfr == NULL) ||
      (cr == NULL)) {
    return HAL_ERR_PARAM;
  }

  for (uint32_t tb = 0U; tb <= WWDG_TB_MAX; tb++) {
    uint64_t us_per_tick_x = (4096ULL << tb) * 1000000ULL; /* x pclk1_hz */
    /* both rounded down: reset no later than max_us, window open by min_us */
    uint64_t ticks = ((uint64_t)max_us * pclk1_hz) / us_per_tick_x;
    uint64_t early = ((uint64_t)min_us * pclk1_hz) / us_per_tick_x;

    if (ticks > WWDG_TICKS_MAX) {
      continue;
    }
    if ((ticks == 0U) || (early >= ticks)) {
      return HAL_ERR_PARAM; /* below one tick, or the window never opens */
    }
    uint32_t t = WWDG_T_FLOOR + (uint32_t)ticks;
    *cfr = (tb << WWDG_CFR_WDGTB_Pos) | (t - (uint32_t)early);
    *cr = (uint8_t)(WWDG_CR_WDGA | t);
    return HAL_OK;
  }
  return HAL_ERR_PARAM; /* longer than 64 ticks at /8 */
}
