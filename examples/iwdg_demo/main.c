/**
 * @file main.c
 * @brief IWDG demo: feed for 5 s, then hang and let the watchdog reset the chip
 *
 * 500 ms watchdog, fed every 100 ms while PC13 toggles. After 5 s the loop feeds
 * once more, drives PC13 high at that instant and hangs. The reset then lands
 * in main(), whose first act drives PC13 low: on a logic analyzer the PC13
 * high-to-low gap is the real timeout, so f_LSI = 40 kHz * 500 / gap_ms.
 * USART1 (PA9, 115200) prints the boot cause each time.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_iwdg.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define LED_PORT GPIOC
#define LED_PIN 13U
#define UART_PORT GPIOA
#define UART_TX 9U
#define TIMEOUT_MS 500U
#define FEED_MS 100U
#define RUN_MS 5000U

void SystemInit(void) {}

int main(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  /* reset marker first, before the slow clock setup */
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_gpio_pin_write(LED_PORT, LED_PIN, false);

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_uart_init(USART1, &uart_cfg);

  hal_uart_write_string(USART1, hal_iwdg_caused_reset()
                                    ? "boot: watchdog reset\r\n"
                                    : "boot: power-on / pin reset\r\n");

  hal_iwdg_freeze_in_debug();
  if (hal_iwdg_start(IWDG, TIMEOUT_MS) != HAL_OK) {
    hal_uart_write_string(USART1, "iwdg start failed\r\n");
  }

  uint32_t boot = hal_get_tick();
  uint32_t last_feed = boot;

  while (1) {
    if (!hal_tick_expired(hal_get_tick(), last_feed, FEED_MS)) {
      continue;
    }
    last_feed = hal_get_tick();

    if ((last_feed - boot) < RUN_MS) {
      hal_iwdg_feed(IWDG);
      hal_gpio_pin_toggle(LED_PORT, LED_PIN);
    } else {
      hal_gpio_pin_write(LED_PORT, LED_PIN, false);
      hal_iwdg_feed(IWDG);
      hal_gpio_pin_write(LED_PORT, LED_PIN, true); /* timeout starts here */
      hal_uart_write_string(USART1, "hanging\r\n");
      for (;;) {
      }
    }
  }
}
