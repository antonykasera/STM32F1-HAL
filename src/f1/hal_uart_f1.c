/**
 * @file hal_uart_f1.c
 * @brief STM32F1 UART (USART async) implementation
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_dma.h"
#include "hal_error.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

HAL_Status hal_uart_init(USART_TypeDef *uart, const UART_Config *cfg) {
  HAL_ASSERT(uart == USART1 || uart == USART2 || uart == USART3);

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

  uint16_t brr = hal_uart_calc_brr(pclk, cfg->baud);
  HAL_ASSERT(brr != 0U); /* baud above pclk / 16 or below pclk / 65535 */
  if (brr == 0U) {
    return HAL_ERR_PARAM;
  }
  uart->BRR = brr;

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
  return HAL_OK;
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

HAL_Status hal_uart_try_read_byte(USART_TypeDef *uart, uint8_t *byte) {
  if (!(uart->SR & USART_SR_RXNE)) {
    return HAL_ERR_EMPTY;
  }
  *byte = (uint8_t)uart->DR;
  return HAL_OK;
}

void hal_uart_flush(USART_TypeDef *uart) {
  while (!(uart->SR & USART_SR_TC)) {
  }
}

/* fixed DMA1 request wiring (RM0008) */
static DMA_Channel_TypeDef *uart_tx_dma(const USART_TypeDef *uart) {
  if (uart == USART1) {
    return DMA1_Channel4;
  }
  if (uart == USART2) {
    return DMA1_Channel7;
  }
  return DMA1_Channel2;
}

HAL_Status hal_uart_write_dma(USART_TypeDef *uart, const uint8_t *buf,
                              uint16_t len) {
  static const DMA_Config cfg = {DMA_DIR_MEM_TO_PERIPH, DMA_SIZE_8, DMA_SIZE_8,
                                 false, true, false, DMA_PRIO_LOW};
  HAL_ASSERT(uart == USART1 || uart == USART2 || uart == USART3);

  if (hal_uart_tx_busy(uart)) {
    return HAL_ERR_BUSY;
  }
  if (len == 0U) {
    return HAL_OK;
  }

  uart->CR3 |= USART_CR3_DMAT;
  uart->SR = ~(uint32_t)USART_SR_TC; /* rc_w0, RM0008 DMA TX step 6 */
  hal_dma_start(uart_tx_dma(uart), &cfg, &uart->DR, (void *)(uintptr_t)buf,
                len);
  return HAL_OK;
}

/* DMA done means the bytes reached DR; TC means the last one left the pin */
bool hal_uart_tx_busy(const USART_TypeDef *uart) {
  const DMA_Channel_TypeDef *ch = uart_tx_dma(uart);

  if ((ch->CCR & DMA_CCR_EN) == 0U) {
    return false; /* never started */
  }
  return (hal_dma_remaining(ch) != 0U) || ((uart->SR & USART_SR_TC) == 0U);
}

void hal_uart_write(USART_TypeDef *uart, const uint8_t *buf, size_t len) {
  for (size_t i = 0U; i < len; i++) {
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
  HAL_ASSERT(uart == USART1 || uart == USART2 || uart == USART3);

  /* RM0008: wait for TC before disabling, or the last byte is cut off.
   * Skipped if never enabled: TC would then never set. */
  if (uart->CR1 & USART_CR1_UE) {
    hal_uart_flush(uart);
  }
  uart->CR1 &= ~USART_CR1_UE;
  if (uart == USART1) {
    RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
  } else if (uart == USART2) {
    RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
  } else {
    RCC->APB1ENR &= ~RCC_APB1ENR_USART3EN;
  }
}
