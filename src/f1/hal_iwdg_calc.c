/**
 * @file hal_iwdg_calc.c
 * @brief IWDG timeout arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * timeout = (4 << PR) * (RLR + 1) / f_LSI  (RM0008)
 */

#include "hal_iwdg.h"
#include <stddef.h>
#include <stdint.h>

#define IWDG_PR_MAX 6U     /* /256 */
#define IWDG_RLR_MAX 0xFFFU /* 12-bit reload */

HAL_Status hal_iwdg_calc(uint32_t timeout_ms, uint8_t *pr, uint16_t *rlr) {
  if ((timeout_ms == 0U) || (pr == NULL) || (rlr == NULL)) {
    return HAL_ERR_PARAM;
  }

  for (uint32_t p = 0U; p <= IWDG_PR_MAX; p++) {
    uint32_t div = 4U << p;
    uint64_t counts = ((uint64_t)timeout_ms * IWDG_LSI_HZ) / (1000U * div);
    if ((counts >= 1U) && (counts <= (IWDG_RLR_MAX + 1U))) {
      *pr = (uint8_t)p;
      *rlr = (uint16_t)(counts - 1U);
      return HAL_OK;
    }
  }
  return HAL_ERR_PARAM; /* longer than /256 with a full reload */
}
