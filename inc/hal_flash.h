/**
 * @file hal_flash.h
 * @brief On-chip flash: page erase and half-word programming
 *
 * 1 KB pages. Erase takes 20-40 ms, a half-word 40-70 us (datasheet), and any
 * flash fetch stalls the core meanwhile: with code and vectors in flash, IRQs
 * wait and SysTick drops up to ~40 ms of ticks per erase. Needs the HSI on
 * (flex never turns it off). Each call unlocks, works, then relocks.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define FLASH_PAGE_BYTES 1024U

#ifndef HAL_FLASH_BYTES
#define HAL_FLASH_BYTES (64U * 1024U) /* C8; match linker.ld */
#endif

#define FLASH_LAST_PAGE ((uint32_t)FLASH_BASE + HAL_FLASH_BYTES - FLASH_PAGE_BYTES)

/* page_addr: first byte of a page; erasing the page you run from crashes */
HAL_Status hal_flash_erase_page(FLASH_TypeDef *flash, uint32_t page_addr);

/* dst and len even. Half-words already equal to src are skipped, others must
 * be erased (0xFFFF). Stops at the first that doesn't stick: HAL_ERR_WRITE */
HAL_Status hal_flash_write(FLASH_TypeDef *flash, void *dst, const void *src,
                           size_t len);

#endif /* HAL_FLASH_H */
