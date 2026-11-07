/**
 * @file main.c
 * @brief UART TX by DMA: the CPU keeps working while a line goes out
 *
 * Every 500 ms queues one line on USART1 (PA9, 115200, ~7 ms on the wire) with
 * hal_uart_write_dma, then toggles PC13 as fast as it can until the transfer
 * is done. The next line reports how many toggles that was. On a logic
 * analyzer PC13 toggles all through the PA9 burst; a blocking write would
 * leave it still.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define LED_PORT GPIOC
#define LED_PIN 13U
#define UART_PORT GPIOA
#define UART_TX 9U

void SystemInit(void) {}

static uint8_t line[96]; /* owned by the DMA until tx_busy is false */

static uint16_t append(uint16_t at, const char *s) {
  while ((*s != '\0') && (at < sizeof(line))) {
    line[at++] = (uint8_t)*s++;
  }
  return at;
}

static uint16_t append_u32(uint16_t at, uint32_t v) {
  char buf[11];
  uint32_t i = 10U;
  buf[i] = '\0';
  do {
    buf[--i] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v > 0U);
  return append(at, &buf[i]);
}

int main(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);

  uint32_t toggles = 0U;

  while (1) {
    uint16_t n = append(0U, "uart dma: PC13 toggled ");
    n = append_u32(n, toggles);
    n = append(n, " times while the previous line was sending\r\n");
    (void)hal_uart_write_dma(USART1, line, n);

    toggles = 0U;
    while (hal_uart_tx_busy(USART1)) {
      hal_gpio_pin_toggle(LED_PORT, LED_PIN);
      toggles++;
    }

    hal_delay_ms(500U);
  }
}
