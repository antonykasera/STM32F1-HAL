/**
 * @file hal_uart.h
 * @brief UART API header file
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  UART_PARITY_NONE,
  UART_PARITY_EVEN,
  UART_PARITY_ODD
} UART_Parity;

typedef enum { UART_STOP_1, UART_STOP_2 } UART_StopBits;

typedef struct {
  uint32_t baud;
  UART_Parity parity;
  UART_StopBits stop;
} UART_Config;

void hal_uart_init(USART_TypeDef *uart, const UART_Config *cfg);

void hal_uart_deinit(USART_TypeDef *uart);

void hal_uart_write_byte(USART_TypeDef *uart, uint8_t byte);

uint8_t hal_uart_read_byte(USART_TypeDef *uart);

/* non-blocking: HAL_OK with the byte, or HAL_ERR_EMPTY if none has arrived */
HAL_Status hal_uart_try_read_byte(USART_TypeDef *uart, uint8_t *byte);

/* wait until the last byte has left the pin, not just been queued */
void hal_uart_flush(USART_TypeDef *uart);

/* non-blocking send by DMA; buf must stay unchanged until tx_busy is false.
 * HAL_ERR_BUSY while a previous one is still going out. */
HAL_Status hal_uart_write_dma(USART_TypeDef *uart, const uint8_t *buf,
                              uint16_t len);
bool hal_uart_tx_busy(USART_TypeDef *uart);

void hal_uart_write(USART_TypeDef *uart, const uint8_t *buf, uint32_t len);

void hal_uart_write_string(USART_TypeDef *uart, const char *s);

uint16_t hal_uart_calc_brr(uint32_t pclk_hz, uint32_t baud);

#endif /* HAL_UART_H */
