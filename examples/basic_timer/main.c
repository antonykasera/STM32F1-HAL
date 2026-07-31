/**
 * @file main.c
 * @brief Blink example using the HAL TIMER module
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_timer.h"

#define LED_PORT GPIOC
#define LED_PIN 13U
#define BLINK_TIMER TIM3
#define TIMER_FREQ 1U

void SystemInit(void) {}

// timer interrupt callback function
void on_timer(void) { hal_gpio_pin_toggle(LED_PORT, LED_PIN); }

int main(void) {

  //  onboard led pin config as an output push/pull, slow speed configuration,
  //  no internal pullup
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  // initialize the system clock using the High speed external crystal if
  // present (8MHz is the default)
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);

  // enable the clock on the target port, GPIOC. We really love blinky :)
  hal_gpio_init_port_clock(LED_PORT);

  // pin to be initialized with the config above
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);

  // init the timer with a desired timer clock frequency in Hz
  hal_timer_init_hz(BLINK_TIMER, TIMER_FREQ);
  // attach the callback that will be fired by the timer interrupt
  hal_timer_register_callback(BLINK_TIMER, on_timer);
  // arm the timer to start ticking away!
  hal_timer_start(BLINK_TIMER);

  // nothing in the superloop because we are handling the blink purely based on
  // the ISR
  while (1) {
  }
}
