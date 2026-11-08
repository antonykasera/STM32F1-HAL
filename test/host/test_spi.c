/**
 * @file test_spi.c
 * @brief Host-side unit tests for the F1 SPI clock/CR1 math and transfer loop
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * SCK = pclk / (2 << BR), master max 18 MHz (datasheet).
 */

#include "hal_spi.h"
#include "test_assert.h"

static void check_br(uint32_t pclk, uint32_t max, uint8_t br, uint32_t hz) {
  uint8_t got_br = 0xFFU;
  uint32_t got_hz = 0U;

  ASSERT_EQ(hal_spi_calc_br(pclk, max, &got_br, &got_hz), HAL_OK);
  ASSERT_EQ(got_br, br);
  ASSERT_EQ(got_hz, hz);
}

void test_spi_calc_br(void) {
  check_br(72000000U, 18000000U, 1U, 18000000U); /* SPI1: /4 */
  check_br(72000000U, 40000000U, 1U, 18000000U); /* capped at 18 MHz */
  check_br(72000000U, 10000000U, 2U, 9000000U);  /* /8 */
  check_br(72000000U, 1000000U, 6U, 562500U);    /* /64 = 1.125 MHz > 1 */
  check_br(36000000U, 18000000U, 0U, 18000000U); /* SPI2: /2 */
}

void test_spi_calc_br_rejects(void) {
  uint8_t br = 0x77U;
  uint32_t hz = 7U;

  ASSERT_EQ(hal_spi_calc_br(72000000U, 0U, &br, &hz), HAL_ERR_PARAM);
  /* 72 MHz / 256 = 281 kHz is the slowest */
  ASSERT_EQ(hal_spi_calc_br(72000000U, 200000U, &br, &hz), HAL_ERR_PARAM);
  ASSERT_EQ(br, 0x77U);
}

void test_spi_calc_cr1(void) {
  SPI_Config m3 = {18000000U, SPI_MODE_3, false};
  SPI_Config m0_lsb = {1000000U, SPI_MODE_0, true};

  /* MSTR 0x4 | SSI 0x100 | SSM 0x200 | BR << 3 | CPOL/CPHA | LSBFIRST 0x80 */
  ASSERT_EQ(hal_spi_calc_cr1(&m3, 1U), 0x30FU);
  ASSERT_EQ(hal_spi_calc_cr1(&m0_lsb, 6U), 0x3B4U);
  ASSERT_EQ(hal_spi_calc_cr1(&m3, 1U) & SPI_CR1_SPE, 0U);
}

/* transfer loop on a SPI_TypeDef in host RAM, flags preset so nothing waits */

void test_spi_transfer_loop(void) {
  SPI_TypeDef s = {0};
  const uint8_t tx[3] = {0x11U, 0x22U, 0xA5U};
  uint8_t rx[3] = {0};
  s.SR = SPI_SR_TXE | SPI_SR_RXNE;
  s.CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;

  ASSERT_EQ(hal_spi_transfer(&s, tx, rx, 3U), HAL_OK);
  ASSERT_EQ(s.CR2, 0U);    /* leftover DMA requests switched off */
  ASSERT_EQ(s.DR, 0xA5U);  /* last byte written */
  ASSERT_EQ(rx[2], 0xA5U); /* RAM DR reads back what was written */

  ASSERT_EQ(hal_spi_transfer(&s, NULL, NULL, 1U), HAL_OK);
  ASSERT_EQ(s.DR, 0xFFU); /* tx NULL clocks out 0xFF */
}
