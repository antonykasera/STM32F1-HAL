/**
 * @file hal_spi_f1.c
 * @brief STM32F1 SPI master driver
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_dma.h"
#include "hal_error.h"
#include "hal_spi.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* a master can't stall once enabled; this only guards misconfiguration */
#define SPI_TIMEOUT_MS 10U

static uint8_t spi_dummy_tx = 0xFFU;
static uint8_t spi_dummy_rx;

static HAL_Status spi_wait(const SPI_TypeDef *spi, uint32_t flag,
                           uint32_t want) {
  uint32_t start = hal_get_tick();

  while ((spi->SR & flag) != want) {
    if (hal_tick_expired(hal_get_tick(), start, SPI_TIMEOUT_MS)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return HAL_OK;
}

HAL_Status hal_spi_init(SPI_TypeDef *spi, const SPI_Config *cfg) {
  HAL_ASSERT((spi == SPI1) || (spi == SPI2));
  if ((spi != SPI1) && (spi != SPI2)) {
    return HAL_ERR_UNSUPPORTED;
  }

  uint32_t pclk;
  if (spi == SPI1) {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    pclk = hal_clock_get_pclk2_hz();
  } else {
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    pclk = hal_clock_get_pclk1_hz();
  }

  uint8_t br = 0U;
  uint32_t actual = 0U;
  HAL_Status st = hal_spi_calc_br(pclk, cfg->max_hz, &br, &actual);
  HAL_ASSERT(st == HAL_OK);
  if (st != HAL_OK) {
    return st;
  }

  spi->CR1 = 0U; /* BR/mode only change with SPE off */
  spi->CR2 = 0U;
  uint32_t cr1 = hal_spi_calc_cr1(cfg, br);
  spi->CR1 = cr1;
  spi->CR1 = cr1 | SPI_CR1_SPE;
  return HAL_OK;
}

void hal_spi_deinit(SPI_TypeDef *spi) {
  (void)spi_wait(spi, SPI_SR_TXE, SPI_SR_TXE);
  (void)spi_wait(spi, SPI_SR_BSY, 0U);
  spi->CR1 &= ~SPI_CR1_SPE;
  if (spi == SPI1) {
    RCC->APB2ENR &= ~RCC_APB2ENR_SPI1EN;
  } else {
    RCC->APB1ENR &= ~RCC_APB1ENR_SPI2EN;
  }
}

HAL_Status hal_spi_transfer(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx,
                            size_t len) {
  spi->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);

  for (size_t i = 0U; i < len; i++) {
    HAL_Status st = spi_wait(spi, SPI_SR_TXE, SPI_SR_TXE);
    if (st != HAL_OK) {
      return st;
    }
    spi->DR = (tx != NULL) ? tx[i] : 0xFFU;

    st = spi_wait(spi, SPI_SR_RXNE, SPI_SR_RXNE);
    if (st != HAL_OK) {
      return st;
    }
    uint8_t b = (uint8_t)spi->DR;
    if (rx != NULL) {
      rx[i] = b;
    }
  }
  return spi_wait(spi, SPI_SR_BSY, 0U);
}

/* fixed DMA1 request wiring (RM0008) */
static DMA_Channel_TypeDef *spi_rx_dma(const SPI_TypeDef *spi) {
  return (spi == SPI1) ? DMA1_Channel2 : DMA1_Channel4;
}

static DMA_Channel_TypeDef *spi_tx_dma(const SPI_TypeDef *spi) {
  return (spi == SPI1) ? DMA1_Channel3 : DMA1_Channel5;
}

/* RX always runs (into a dummy byte if rx is NULL): TX-only leaves OVR set */
HAL_Status hal_spi_transfer_dma(SPI_TypeDef *spi, const uint8_t *tx,
                                uint8_t *rx, uint16_t len) {
  HAL_ASSERT((spi == SPI1) || (spi == SPI2));

  if (hal_spi_busy(spi)) {
    return HAL_ERR_BUSY;
  }
  if (len == 0U) {
    return HAL_OK;
  }

  DMA_Config rx_cfg = {DMA_DIR_PERIPH_TO_MEM, DMA_SIZE_8, DMA_SIZE_8,
                       false, rx != NULL, false, DMA_PRIO_HIGH};
  DMA_Config tx_cfg = {DMA_DIR_MEM_TO_PERIPH, DMA_SIZE_8, DMA_SIZE_8,
                       false, tx != NULL, false, DMA_PRIO_MEDIUM};

  (void)spi->DR; /* drop a stale RXNE */
  (void)spi->SR;
  hal_dma_start(spi_rx_dma(spi), &rx_cfg, &spi->DR,
                (rx != NULL) ? rx : &spi_dummy_rx, len);
  hal_dma_start(spi_tx_dma(spi), &tx_cfg, &spi->DR,
                (tx != NULL) ? (void *)(uintptr_t)tx : &spi_dummy_tx, len);
  spi->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN; /* TXE starts it at once */
  return HAL_OK;
}

/* RX count reaching 0 means every byte came back; BSY covers the tail */
bool hal_spi_busy(const SPI_TypeDef *spi) {
  const DMA_Channel_TypeDef *rx = spi_rx_dma(spi);

  if ((rx->CCR & DMA_CCR_EN) == 0U) {
    return false; /* never started */
  }
  return (hal_dma_remaining(rx) != 0U) || ((spi->SR & SPI_SR_BSY) != 0U);
}
