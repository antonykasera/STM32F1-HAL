/**
 * @file test_crc.c
 * @brief Host-side unit tests for the CRC model and register sequence
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Vectors cross-checked in Python against zlib on bit-reversed words.
 */

#include "hal_crc.h"
#include "test_assert.h"

void test_crc_calc_vectors(void) {
  const uint32_t one = 0x12345678U;
  const uint32_t zero = 0U;
  const uint32_t two[2] = {0x12345678U, 0x9ABCDEF0U};

  ASSERT_EQ(hal_crc_calc(HAL_CRC_INIT, &one, 1U), 0xDF8A8A2BU);
  ASSERT_EQ(hal_crc_calc(HAL_CRC_INIT, &zero, 1U), 0xC704DD7BU);
  ASSERT_EQ(hal_crc_calc(HAL_CRC_INIT, two, 2U), 0x7D24A31BU);
  ASSERT_EQ(hal_crc_calc(HAL_CRC_INIT, NULL, 0U), HAL_CRC_INIT);
}

void test_crc_calc_chains(void) {
  const uint32_t two[2] = {0x12345678U, 0x9ABCDEF0U};
  uint32_t first = hal_crc_calc(HAL_CRC_INIT, &two[0], 1U);

  ASSERT_EQ(hal_crc_calc(first, &two[1], 1U), 0x7D24A31BU);
}

/* register-level, on a CRC_TypeDef in host RAM (no real CRC math) */

void test_crc_reset_and_feed(void) {
  CRC_TypeDef c = {0};
  const uint32_t words[3] = {1U, 2U, 3U};

  hal_crc_reset(&c);
  ASSERT_EQ(c.CR, CRC_CR_RESET);
  ASSERT_EQ(hal_crc_feed(&c, words, 3U), 3U); /* RAM DR: last word written */
}
