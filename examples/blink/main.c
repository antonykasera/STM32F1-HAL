/**
 * @file main.c
 * @brief Blink example using the HAL
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "board.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define LED_PIN 13U
#define LED_PORT GPIOC

void SystemInit(void) {}

int main(void) {

  // simple LED config, self explanatory, push/pull output pin, slow speed, no
  // internal pull up/pull down resistor needed for the blink
  GPIO_Config LED_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  const uint32_t delay_ms = 1000U;

  // initialize the system clock
  hal_sysclock_init();
  // initialize system tick to allow the use of the delay function
  hal_systick_init();

  // enable the clock on the target port, GPIOC
  hal_gpio_init_port_clock(LED_PORT);

  // initialize the target pin with the config defined
  hal_gpio_pin_init(LED_PORT, LED_PIN, &LED_cfg);

  while (1) {
    // LED on
    hal_gpio_pin_write(LED_PORT, LED_PIN, 0U);
    hal_delay_ms(delay_ms);
    // LED off
    hal_gpio_pin_write(LED_PORT, LED_PIN, 1U);
    hal_delay_ms(delay_ms);
  }
}
