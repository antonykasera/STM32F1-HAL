/**
 * @file hal_crc_calc.c
 * @brief Bitwise model of the CRC unit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_crc.h"
#include "hal_error.h"
#include <stddef.h>
#include <stdint.h>

#define CRC_POLY 0x04C11DB7U

uint32_t hal_crc_calc(uint32_t crc, const uint32_t *words, size_t n) {
  HAL_ASSERT((words != NULL) || (n == 0U));

  for (size_t i = 0U; i < n; i++) {
    crc ^= words[i];
    for (uint32_t bit = 0U; bit < 32U; bit++) {
      crc = ((crc & 0x80000000U) != 0U) ? ((crc << 1) ^ CRC_POLY) : (crc << 1);
    }
  }
  return crc;
}
