/**
 * @file main.c
 * @brief UART example "Hello World!"
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_RX 10U
#define UART_TX 9U
#define PORT GPIOA

void SystemInit(void) {}

int main(void) {
  // 2 pin configs for the USART1 rx and tx pins
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config rx_cfg = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  // UART config to specify baudrate, parity and stop bit
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  // initialize system clock and system tick
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  // enable clock on the USART1 port, GPIOA
  hal_gpio_init_port_clock(PORT);

  // initialize the pins with the configs
  hal_gpio_pin_init(PORT, UART_RX, &rx_cfg);
  hal_gpio_pin_init(PORT, UART_TX, &tx_cfg);

  // enable alternate function IO clock on the TX pin
  hal_gpio_alternate_en(PORT, UART_TX);

  // initialize USART1 with the config
  hal_uart_init(USART1, &uart_cfg);

  while (1) {
    // Hello World! ;)
    hal_uart_write_string(USART1, "Hello World!\r\n");
    hal_delay_ms(1000U);
  }
}
