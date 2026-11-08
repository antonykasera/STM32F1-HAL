/**
 * @file main.c
 * @brief WWDG demo: refresh inside the window for 2 s, then break it
 *
 * Window 20-50 ms, really 19.1-49.2 ms (0.91 ms ticks at PCLK1 = 36 MHz),
 * refreshed every ~31 ms while PC13 toggles. After 2 s:
 *   FAULT_EARLY 1: refresh again 5 ms later, inside the closed part: resets
 *                  at once, so PC13 high -> low is just the reboot
 *   FAULT_EARLY 0: stop refreshing: PC13 high -> low is the real timeout
 * main() drives PC13 low first thing, which marks each reboot. USART1 (PA9,
 * 115200) prints the boot cause.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_wwdg.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define FAULT_EARLY 1
#define LED_PORT GPIOC
#define LED_PIN 13U
#define UART_PORT GPIOA
#define UART_TX 9U
#define WINDOW_MIN_US 20000U
#define WINDOW_MAX_US 50000U
#define REFRESH_MS 30U
#define RUN_MS 2000U

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

  hal_uart_write_string(USART1, hal_wwdg_caused_reset()
                                    ? "boot: window watchdog reset\r\n"
                                    : "boot: power-on / pin reset\r\n");

  hal_wwdg_freeze_in_debug();
  if (hal_wwdg_start(WWDG, WINDOW_MIN_US, WINDOW_MAX_US) != HAL_OK) {
    hal_uart_write_string(USART1, "wwdg start failed\r\n");
  }

  uint32_t boot = hal_get_tick();
  uint32_t last = boot;

  while (1) {
    if (!hal_tick_expired(hal_get_tick(), last, REFRESH_MS)) {
      continue;
    }
    last = hal_get_tick();

    if ((last - boot) < RUN_MS) {
      hal_wwdg_refresh(WWDG);
      hal_gpio_pin_toggle(LED_PORT, LED_PIN);
    } else {
      hal_gpio_pin_write(LED_PORT, LED_PIN, false);
      hal_uart_write_string(USART1, FAULT_EARLY ? "refreshing too early\r\n"
                                                : "no more refreshes\r\n");
      hal_wwdg_refresh(WWDG);
#if FAULT_EARLY
      hal_delay_ms(5U);
#endif
      hal_gpio_pin_write(LED_PORT, LED_PIN, true);
#if FAULT_EARLY
      hal_wwdg_refresh(WWDG); /* counter still above W */
#endif
      for (;;) {
      }
    }
  }
}
