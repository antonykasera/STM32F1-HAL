/**
 * @file hal_uart_calc.c
 * @brief UART baud-rate arithmetic
 *
 * Copyright (c) 2026. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * BRR encodes USARTDIV = f_CK / (16 * baud) as a 12-bit mantissa + 4-bit
 * fraction. Since the fraction is /16, computing (f_CK / baud) directly yields
 * USARTDIV*16 -- which IS the BRR bit layout (mantissa in [15:4], frac in
 * [3:0]). Adding baud/2 before the divide rounds to nearest instead of
 * truncating.
 */

#include "hal_uart.h"
#include <stdint.h>

uint16_t hal_uart_calc_brr(uint32_t pclk_hz, uint32_t baud) {
  if (baud == 0U) {
    return 0U;
  }

  uint32_t brr = (pclk_hz + (baud / 2U)) / baud;

  return (uint16_t)brr;
}
