/**
 * @file main.c
 * @brief USB CDC echo: a virtual serial port that types back what you send
 *
 * Plug the Blue Pill's own USB port into the PC (the ST-Link alone doesn't
 * reach it), then open the port at any baud, e.g. `picocom /dev/ttyACM0`.
 * Opening it prints a banner, and every key comes back (Enter as CR LF).
 * PC13 LED on = enumerated. `lsusb` shows 1209:0001; `dmesg` names the tty.
 *
 * USB_IRQ 1 serves USB from its interrupt instead of the loop.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_usb.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_IRQ 0
#define LED_PORT GPIOC
#define LED_PIN 13U

void SystemInit(void) {}

static void print(const char *s) {
  size_t n = 0U;
  while (s[n] != '\0') {
    n++;
  }
  (void)hal_usb_cdc_write(s, n);
}

static void print_u32(uint32_t v) {
  char buf[11];
  uint32_t i = 10U;
  buf[i] = '\0';
  do {
    buf[--i] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v > 0U);
  print(&buf[i]);
}

int main(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE); /* USB needs the 72 MHz PLL */
  hal_systick_init();

  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_gpio_pin_write(LED_PORT, LED_PIN, true); /* off (active low) */

  hal_usb_init();
#if USB_IRQ
  hal_usb_enable_interrupt(8U);
#endif

  bool was_open = false;
  while (1) {
    hal_usb_poll(); /* in IRQ mode this just nudges the ISR */
    hal_gpio_pin_write(LED_PORT, LED_PIN, !hal_usb_configured());

    bool open = hal_usb_cdc_dtr();
    if (open && !was_open) {
      print("\r\nflex usb cdc echo (");
      print(USB_IRQ ? "irq" : "polled");
      print("), host baud ");
      print_u32(hal_usb_cdc_baud());
      print("\r\n");
    }
    was_open = open;

    uint8_t in[16];
    size_t n = hal_usb_cdc_read(in, sizeof in);
    for (size_t i = 0U; i < n; i++) {
      (void)hal_usb_cdc_write(&in[i], 1U);
      if (in[i] == '\r') {
        (void)hal_usb_cdc_write("\n", 1U);
      }
    }
  }
}
