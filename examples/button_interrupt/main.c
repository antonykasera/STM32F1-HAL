/**
 * @file main.c
 * @brief Button interrupt example using the HAL
 *
 * Copyright (c) 2026. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * RC low pass filter or some form of debouncing is expected.
 * Values used:
 * R=10k
 * C=100nF
 */

#include "board.h"
#include "hal_clock.h"
#include "hal_exti.h"
#include "hal_gpio.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define BTN_PORT GPIOB
#define BTN_PIN 1U
#define DELAY_MS 500U

#define LED_PIN 13U
#define LED_PORT GPIOC

volatile bool isPressed = false;

// callback function after the interrupt fires
void blink_LED(void) { isPressed = true; }

void SystemInit(void) {};
int main(void) {
  // button pin config as a input pull pin, input speed, internal
  // pull up configured
  GPIO_Config button_cfg = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT,
                            GPIO_PULL_UP};

  // led pin config as an output push/pull, slow speed configuration, no
  // internal pullup
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  // initialize the sysem clock and system ticker for the delay function
  hal_sysclock_init();
  hal_systick_init();

  // enable clock on the button and led ports
  hal_gpio_init_port_clock(BTN_PORT);
  hal_gpio_init_port_clock(LED_PORT);

  // initialize the pins with the configs
  hal_gpio_pin_init(BTN_PORT, BTN_PIN, &button_cfg);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);

  // initialize pin as alternate function and specify
  // edge trigger type
  hal_exti_init(BTN_PORT, BTN_PIN, FALLING_EDGE);

  // attach the function callback to the interrupt line
  hal_exti_register_callback(BTN_PIN, blink_LED);

  hal_gpio_pin_write(LED_PORT, LED_PIN, 1U);
  while (1) {

    // will only evaluate to true when
    // the interrupt flips the state of the isPressed flag
    if (isPressed) {
      isPressed = false;
      for (uint8_t i = 0; i < 10; i++) {
        hal_gpio_pin_write(LED_PORT, LED_PIN, 0U);
        hal_delay_ms(DELAY_MS);
        hal_gpio_pin_write(LED_PORT, LED_PIN, 1U);
        hal_delay_ms(DELAY_MS);
      }
      hal_gpio_pin_write(LED_PORT, LED_PIN, 1U);
    }
  }
}
