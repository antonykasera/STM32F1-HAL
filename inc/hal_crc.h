/**
 * @file hal_crc.h
 * @brief CRC unit: CRC-32 (poly 0x04C11DB7) over 32-bit words
 *
 * Starts at 0xFFFFFFFF, shifts MSB first, no reflection and no final XOR, so
 * it does NOT match zlib's crc32(). The word 0x12345678 alone gives 0xDF8A8A2B.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_CRC_H
#define HAL_CRC_H

#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define HAL_CRC_INIT 0xFFFFFFFFU

/* software model of the unit, same result; pass HAL_CRC_INIT to start */
uint32_t hal_crc_calc(uint32_t crc, const uint32_t *words, size_t n);

void hal_crc_init(void); /* clock on */
void hal_crc_reset(CRC_TypeDef *crc);

/* continues from the current value; returns the running CRC */
uint32_t hal_crc_feed(CRC_TypeDef *crc, const uint32_t *words, size_t n);

#endif /* HAL_CRC_H */
