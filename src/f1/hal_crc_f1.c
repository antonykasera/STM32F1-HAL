/**
 * @file hal_crc_f1.c
 * @brief STM32F1 CRC unit driver
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_crc.h"
#include "hal_error.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

void hal_crc_init(void) { RCC->AHBENR |= RCC_AHBENR_CRCEN; }

void hal_crc_reset(CRC_TypeDef *crc) { crc->CR = CRC_CR_RESET; }

uint32_t hal_crc_feed(CRC_TypeDef *crc, const uint32_t *words, size_t n) {
  HAL_ASSERT((words != NULL) || (n == 0U));

  /* each write stalls the bus until the 4-cycle update is done */
  for (size_t i = 0U; i < n; i++) {
    crc->DR = words[i];
  }
  return crc->DR;
}
