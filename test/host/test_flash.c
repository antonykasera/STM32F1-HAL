/**
 * @file test_flash.c
 * @brief Host-side unit tests for the F1 flash erase/program sequences
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * A FLASH_TypeDef in host RAM never goes BSY, and a RAM buffer stands in for
 * the flash being written.
 */

#include "hal_flash.h"
#include "test_assert.h"

void test_flash_erase_page(void) {
  FLASH_TypeDef f = {0};
  f.CR = FLASH_CR_LOCK;

  ASSERT_EQ(hal_flash_erase_page(&f, FLASH_LAST_PAGE), HAL_OK);
  ASSERT_EQ(f.KEYR, FLASH_KEY2); /* unlocked, second key last */
  ASSERT_EQ(f.AR, 0x0800FC00U);
  ASSERT_EQ(f.CR, FLASH_CR_LOCK); /* PER off, relocked */
}

void test_flash_erase_reports_protection(void) {
  FLASH_TypeDef f = {0};
  f.SR = FLASH_SR_WRPRTERR;

  ASSERT_EQ(hal_flash_erase_page(&f, FLASH_BASE), HAL_ERR_WRITE);
  ASSERT_EQ(f.CR, FLASH_CR_LOCK);
}

void test_flash_write_halfwords(void) {
  FLASH_TypeDef f = {0}; /* already unlocked: no keys written */
  uint16_t mem[3] = {0xFFFFU, 0xFFFFU, 0xFFFFU};
  const uint8_t src[6] = {0x34U, 0x12U, 0x78U, 0x56U, 0xBCU, 0x9AU};

  ASSERT_EQ(hal_flash_write(&f, mem, src, sizeof src), HAL_OK);
  ASSERT_EQ(mem[0], 0x1234U); /* little-endian */
  ASSERT_EQ(mem[1], 0x5678U);
  ASSERT_EQ(mem[2], 0x9ABCU);
  ASSERT_EQ(f.KEYR, 0U);
  ASSERT_EQ(f.CR, FLASH_CR_LOCK);
}

void test_flash_write_stops_on_error(void) {
  FLASH_TypeDef f = {0};
  uint16_t mem[2] = {0xFFFFU, 0xFFFFU};
  const uint16_t src[2] = {0x1111U, 0x2222U};
  f.SR = FLASH_SR_PGERR;

  ASSERT_EQ(hal_flash_write(&f, mem, src, sizeof src), HAL_ERR_WRITE);
  ASSERT_EQ(mem[1], 0xFFFFU); /* second half-word never attempted */
  ASSERT_EQ(f.CR, FLASH_CR_LOCK);
}

void test_flash_write_skips_equal(void) {
  FLASH_TypeDef f = {0};
  uint16_t mem[2] = {0x1111U, 0x2222U};
  const uint16_t src[2] = {0x1111U, 0x2222U};
  f.SR = FLASH_SR_PGERR; /* would fail if anything were programmed */

  ASSERT_EQ(hal_flash_write(&f, mem, src, sizeof src), HAL_OK);
}
