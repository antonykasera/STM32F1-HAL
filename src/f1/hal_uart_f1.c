/**
 * @file hal_uart_f1.c
 * @brief STM32F1 UART (USART async) implementation
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

void hal_uart_init(USART_TypeDef *uart, const UART_Config *cfg) {
  uint32_t pclk;
  if (uart == USART1) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    pclk = hal_clock_get_pclk2_hz();
  } else if (uart == USART2) {
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    pclk = hal_clock_get_pclk1_hz();
  } else {
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    pclk = hal_clock_get_pclk1_hz();
  }

  uart->BRR = hal_uart_calc_brr(pclk, cfg->baud);

  uart->CR2 &= ~USART_CR2_STOP;
  if (cfg->stop == UART_STOP_2) {
    uart->CR2 |= USART_CR2_STOP_1; /* STOP field '10' = 2 stop bits */
  }

  uart->CR1 &= ~(USART_CR1_PCE | USART_CR1_PS | USART_CR1_M);
  if (cfg->parity != UART_PARITY_NONE) {
    uart->CR1 |= USART_CR1_PCE;
    uart->CR1 |= USART_CR1_M; /* 9-bit frame so 8 data bits remain + parity */
    if (cfg->parity == UART_PARITY_ODD) {
      uart->CR1 |= USART_CR1_PS;
    }
  }

  uart->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void hal_uart_write_byte(USART_TypeDef *uart, uint8_t byte) {
  while (!(uart->SR & USART_SR_TXE)) {
  }
  uart->DR = byte;
}

uint8_t hal_uart_read_byte(USART_TypeDef *uart) {
  while (!(uart->SR & USART_SR_RXNE)) {
  }
  return (uint8_t)uart->DR; /* reading DR returns the byte and clears RXNE */
}

void hal_uart_write(USART_TypeDef *uart, const uint8_t *buf, uint32_t len) {
  for (uint32_t i = 0U; i < len; i++) {
    hal_uart_write_byte(uart, buf[i]);
  }
}

void hal_uart_write_string(USART_TypeDef *uart, const char *s) {
  while (*s != '\0') {
    hal_uart_write_byte(uart, (uint8_t)*s);
    s++;
  }
}

void hal_uart_deinit(USART_TypeDef *uart) {
  uart->CR1 &= ~USART_CR1_UE;
  if (uart == USART1) {
    RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
  } else if (uart == USART2) {
    RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
  } else {
    RCC->APB1ENR &= ~RCC_APB1ENR_USART3EN;
  }
}
