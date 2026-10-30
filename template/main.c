/**
 * @file main.c
 * @brief Minimal flex consumer: blink PC13
 *
 * Shows the pattern for using flex as a library from your own project. Your app
 * owns main(), the linker script, and SystemInit(); flex provides the drivers.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define LED_PORT GPIOC
#define LED_PIN 13U

/* The CMSIS startup calls SystemInit() before main(). An empty body is fine.
 * flex configures the clock explicitly via hal_sysclock_init(). */
void SystemInit(void) {}

int main(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);

  while (1) {
    hal_gpio_pin_toggle(LED_PORT, LED_PIN);
    hal_delay_ms(500U);
  }
}
