/**
 * @file hal_flash_f1.c
 * @brief STM32F1 flash program/erase controller (FPEC) driver
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Sequences from PM0075.
 */

#include "hal_error.h"
#include "hal_flash.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define FLASH_SR_ERRORS (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)

static void flash_unlock(FLASH_TypeDef *flash) {
  /* keys only while locked: a wrong sequence locks the FPEC until reset */
  if ((flash->CR & FLASH_CR_LOCK) != 0U) {
    flash->KEYR = FLASH_KEY1;
    flash->KEYR = FLASH_KEY2;
  }
}

static HAL_Status flash_wait(FLASH_TypeDef *flash) {
  while ((flash->SR & FLASH_SR_BSY) != 0U) {
  }
  uint32_t err = flash->SR & FLASH_SR_ERRORS;
  flash->SR = FLASH_SR_EOP | err; /* rc_w1 */
  return (err != 0U) ? HAL_ERR_WRITE : HAL_OK;
}

HAL_Status hal_flash_erase_page(FLASH_TypeDef *flash, uint32_t page_addr) {
  HAL_ASSERT((page_addr >= FLASH_BASE) &&
             (page_addr < (FLASH_BASE + HAL_FLASH_BYTES)));
  HAL_ASSERT((page_addr % FLASH_PAGE_BYTES) == 0U);

  flash_unlock(flash);
  flash->CR = FLASH_CR_PER;
  flash->AR = page_addr;
  flash->CR = FLASH_CR_PER | FLASH_CR_STRT;
  HAL_Status st = flash_wait(flash);
  flash->CR = FLASH_CR_LOCK; /* also drops PER */
  return st;
}

HAL_Status hal_flash_write(FLASH_TypeDef *flash, void *dst, const void *src,
                           size_t len) {
  HAL_ASSERT((dst != NULL) && ((src != NULL) || (len == 0U)));
  /* the FPEC bus-faults any write that isn't a half-word */
  HAL_ASSERT((((uintptr_t)dst | len) & 1U) == 0U);

  volatile uint16_t *d = dst;
  const uint8_t *s = src; /* src may be unaligned */
  HAL_Status st = HAL_OK;

  flash_unlock(flash);
  flash->CR = FLASH_CR_PG;
  for (size_t i = 0U; (i < (len / 2U)) && (st == HAL_OK); i++) {
    uint16_t v = (uint16_t)(s[2U * i] | (s[(2U * i) + 1U] << 8));
    if (d[i] != v) {
      d[i] = v;
      st = flash_wait(flash);
      if ((st == HAL_OK) && (d[i] != v)) {
        st = HAL_ERR_WRITE;
      }
    }
  }
  flash->CR = FLASH_CR_LOCK;
  return st;
}
