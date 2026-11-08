/**
 * @file hal_iwdg_f1.c
 * @brief STM32F1 independent watchdog driver
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_iwdg.h"
#include "hal_reset.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define IWDG_KEY_START 0xCCCCU  /* also forces the LSI on */
#define IWDG_KEY_UNLOCK 0x5555U /* PR and RLR are write-protected */
#define IWDG_KEY_RELOAD 0xAAAAU
#define IWDG_UPDATE_TIMEOUT_MS 10U

HAL_Status hal_iwdg_start(IWDG_TypeDef *iwdg, uint32_t timeout_ms) {
  uint8_t pr = 0U;
  uint16_t rlr = 0U;
  HAL_Status st = hal_iwdg_calc(timeout_ms, &pr, &rlr);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return st;
  }

  iwdg->KR = IWDG_KEY_START;
  iwdg->KR = IWDG_KEY_UNLOCK;
  iwdg->PR = pr;
  iwdg->RLR = rlr;

  /* new values cross into the LSI domain: up to 5 LSI cycles (RM0008) */
  uint32_t start = hal_get_tick();
  while ((iwdg->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) {
    if (hal_tick_expired(hal_get_tick(), start, IWDG_UPDATE_TIMEOUT_MS)) {
      return HAL_ERR_TIMEOUT;
    }
  }

  iwdg->KR = IWDG_KEY_RELOAD;
  return HAL_OK;
}

void hal_iwdg_feed(IWDG_TypeDef *iwdg) { iwdg->KR = IWDG_KEY_RELOAD; }

void hal_iwdg_freeze_in_debug(void) {
  DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;
}

bool hal_iwdg_caused_reset(void) {
  return (hal_reset_cause() & HAL_RESET_IWDG) != 0U;
}
