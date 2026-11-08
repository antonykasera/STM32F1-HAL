/**
 * @file hal_spi_calc.c
 * @brief SPI clock divider and control-register arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * SCK = pclk / (2 << BR), BR 0..7 (RM0008).
 */

#include "hal_spi.h"
#include <stddef.h>
#include <stdint.h>

#define SPI_BR_MAX 7U

HAL_Status hal_spi_calc_br(uint32_t pclk_hz, uint32_t max_hz, uint8_t *br,
                           uint32_t *actual_hz) {
  if ((max_hz == 0U) || (br == NULL) || (actual_hz == NULL)) {
    return HAL_ERR_PARAM;
  }
  uint32_t limit = (max_hz < SPI_SCK_MAX_HZ) ? max_hz : SPI_SCK_MAX_HZ;

  for (uint32_t b = 0U; b <= SPI_BR_MAX; b++) {
    uint32_t sck = pclk_hz / (2U << b);
    if (sck <= limit) {
      *br = (uint8_t)b;
      *actual_hz = sck;
      return HAL_OK;
    }
  }
  return HAL_ERR_PARAM; /* slower than pclk / 256 */
}

/* SSM + SSI: NSS held high internally, so master mode can't trip MODF */
uint32_t hal_spi_calc_cr1(const SPI_Config *cfg, uint8_t br) {
  uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                 ((uint32_t)br << SPI_CR1_BR_Pos) | (uint32_t)cfg->mode;

  if (cfg->lsb_first) {
    cr1 |= SPI_CR1_LSBFIRST;
  }
  return cr1;
}
