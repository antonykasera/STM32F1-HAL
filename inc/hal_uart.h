/**
 * @file hal_uart.h
 * @brief UART API header file
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_types.h"
#include "stm32f103xb.h"
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

void hal_uart_write(USART_TypeDef *uart, const uint8_t *buf, uint32_t len);

void hal_uart_write_string(USART_TypeDef *uart, const char *s);

uint16_t hal_uart_calc_brr(uint32_t pclk_hz, uint32_t baud);

#endif /* HAL_UART_H */
