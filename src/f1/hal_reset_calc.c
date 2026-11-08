/**
 * @file hal_reset_calc.c
 * @brief Reset cause decoding
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_reset.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define RESET_FLAGS_MASK 0x3FU /* PINRSTF..LPWRRSTF, bits 26-31 */

uint32_t hal_reset_calc_cause(uint32_t csr) {
  return (csr >> RCC_CSR_PINRSTF_Pos) & RESET_FLAGS_MASK;
}

const char *hal_reset_cause_str(uint32_t cause) {
  if ((cause & HAL_RESET_LOW_POWER) != 0U) {
    return "low-power";
  }
  if ((cause & HAL_RESET_WWDG) != 0U) {
    return "wwdg";
  }
  if ((cause & HAL_RESET_IWDG) != 0U) {
    return "iwdg";
  }
  if ((cause & HAL_RESET_SOFTWARE) != 0U) {
    return "software";
  }
  if ((cause & HAL_RESET_POWER_ON) != 0U) {
    return "power-on";
  }
  if ((cause & HAL_RESET_PIN) != 0U) {
    return "pin";
  }
  return "none";
}
