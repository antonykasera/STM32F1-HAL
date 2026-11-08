/**
 * @file main.c
 * @brief UART echo: non-blocking receive in a superloop
 *
 * Echoes every byte received on USART1 while PC13 blinks at about 2 Hz. The
 * blink keeps its rhythm whether or not bytes arrive, since nothing waits for
 * input. Wiring: PA9 TX, PA10 RX, 115200 8N1.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_PORT GPIOA
#define UART_TX 9U
#define UART_RX 10U
#define LED_PORT GPIOC
#define LED_PIN 13U
#define BLINK_MS 250U

void SystemInit(void) {}

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config rx_cfg = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(UART_PORT, UART_RX, &rx_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_uart_init(USART1, &uart_cfg);

  uint32_t last_blink = hal_get_tick();

  while (1) {
    uint8_t byte;
    if (hal_uart_try_read_byte(USART1, &byte) == HAL_OK) {
      hal_uart_write_byte(USART1, byte);
    }

    if (hal_tick_expired(hal_get_tick(), last_blink, BLINK_MS)) {
      last_blink = hal_get_tick();
      hal_gpio_pin_toggle(LED_PORT, LED_PIN);
    }
  }
}
