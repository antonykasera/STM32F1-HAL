/**
 * @file hal_spi.h
 * @brief SPI master, 8-bit frames, blocking or DMA
 *
 * Chip select is the caller's GPIO (NSS is managed in software). Pins:
 * SPI1 SCK PA5, MISO PA6, MOSI PA7; SPI2 SCK PB13, MISO PB14, MOSI PB15.
 * SCK/MOSI as AF push-pull, MISO as input.
 *
 * DMA channels are fixed (RM0008): SPI1 RX 2 / TX 3, SPI2 RX 4 / TX 5. SPI2 RX
 * shares channel 4 with USART1 TX, so not both by DMA at once.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPI_SCK_MAX_HZ 18000000U /* master, datasheet */

typedef enum {
  SPI_MODE_0, /* CPOL 0, CPHA 0 */
  SPI_MODE_1, /* CPOL 0, CPHA 1 */
  SPI_MODE_2, /* CPOL 1, CPHA 0 */
  SPI_MODE_3, /* CPOL 1, CPHA 1 */
} SPI_Mode;

typedef struct {
  uint32_t max_hz; /* fastest SCK the device allows; capped at 18 MHz */
  SPI_Mode mode;
  bool lsb_first;
} SPI_Config;

HAL_Status hal_spi_init(SPI_TypeDef *spi, const SPI_Config *cfg);
void hal_spi_deinit(SPI_TypeDef *spi);

/* full duplex, len bytes: tx NULL sends 0xFF, rx NULL discards */
HAL_Status hal_spi_transfer(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx,
                            size_t len);

/* same, by DMA; buffers must stay put until busy is false. HAL_ERR_BUSY while
 * a previous one is running. */
HAL_Status hal_spi_transfer_dma(SPI_TypeDef *spi, const uint8_t *tx,
                                uint8_t *rx, uint16_t len);
bool hal_spi_busy(const SPI_TypeDef *spi);

/* BR code for the fastest SCK <= min(max_hz, 18 MHz) */
HAL_Status hal_spi_calc_br(uint32_t pclk_hz, uint32_t max_hz, uint8_t *br,
                           uint32_t *actual_hz);
/* CR1 for a master with software NSS, without SPE */
uint32_t hal_spi_calc_cr1(const SPI_Config *cfg, uint8_t br);

#endif /* HAL_SPI_H */
