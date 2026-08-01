/**
 * @file test_uart.c
 * @brief Host-side unit tests for the F1 UART baud-rate math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Only hal_uart_calc_brr is exercised -- pure arithmetic (pclk + baud -> BRR),
 * no register access. The register driver (hal_uart_f1.c) touches USART/RCC and
 * is NOT host-callable.
 *
 * BRR = round(f_CK / (16 * baud)) laid out as mantissa[15:4] + frac[3:0], which
 * equals round(f_CK / baud). Check values computed from RM0008 USARTDIV.
 */

#include "hal_uart.h"
#include "test_assert.h"

void test_brr_72mhz_115200(void) {
  /* USARTDIV = 72e6/(16*115200) = 39.0625 -> mant 39, frac 1 -> 0x271 */
  ASSERT_EQ(hal_uart_calc_brr(72000000U, 115200U), 0x271U);
}

void test_brr_72mhz_9600(void) {
  /* USARTDIV = 468.75 -> mant 468, frac 12 -> 0x1D4C */
  ASSERT_EQ(hal_uart_calc_brr(72000000U, 9600U), 0x1D4CU);
}

void test_brr_36mhz_115200(void) {
  /* USART2/3 on APB1 (36 MHz): USARTDIV = 19.53 -> mant 19, frac 9 -> 0x139 */
  ASSERT_EQ(hal_uart_calc_brr(36000000U, 115200U), 0x139U);
}

void test_brr_zero_baud_guarded(void) {
  ASSERT_EQ(hal_uart_calc_brr(72000000U, 0U), 0U);
}
