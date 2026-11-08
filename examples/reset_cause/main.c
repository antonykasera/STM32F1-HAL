/**
 * @file main.c
 * @brief Reset cause: walks through software, IWDG and WWDG resets by itself
 *
 * Each boot prints its cause on USART1 (PA9, 115200), then picks the next
 * reset from it, so no state needs to survive:
 *   cause 0x02 power-on  iwdg 0 wwdg 0  -> software reset
 *   cause 0x04 software  iwdg 0 wwdg 0  -> iwdg
 *   cause 0x08 iwdg  iwdg 1 wwdg 0  -> wwdg
 *   cause 0x10 wwdg  iwdg 0 wwdg 1  -> done, press RESET
 * RESET then starts over from 0x01 pin. A chip may also set PIN (0x01) with
 * the others. The iwdg/wwdg columns check that several modules can ask in
 * one boot.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_iwdg.h"
#include "hal_reset.h"
#include "hal_uart.h"
#include "hal_wwdg.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_PORT GPIOA
#define UART_TX 9U

void SystemInit(void) {}

static void print(const char *s) { hal_uart_write_string(USART1, s); }

static void print_hex8(uint8_t v) {
  const char hex[] = "0123456789ABCDEF";
  const char buf[] = {'0', 'x', hex[v >> 4], hex[v & 0xFU], '\0'};
  print(buf);
}

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();
  hal_gpio_init_port_clock(UART_PORT);
  hal_uart_init(USART1, &uart_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);

  uint32_t cause = hal_reset_cause();
  print("cause ");
  print_hex8((uint8_t)cause);
  print(" ");
  print(hal_reset_cause_str(cause));
  print(hal_iwdg_caused_reset() ? "  iwdg 1" : "  iwdg 0");
  print(hal_wwdg_caused_reset() ? " wwdg 1" : " wwdg 0");

  if ((cause & HAL_RESET_WWDG) != 0U) {
    print("  -> done, press RESET\r\n");
    while (1) {
    }
  }

  if ((cause & HAL_RESET_IWDG) != 0U) {
    print("  -> wwdg\r\n");
    hal_uart_flush(USART1);
    (void)hal_wwdg_start(WWDG, 0U, 10000U);
  } else if ((cause & HAL_RESET_SOFTWARE) != 0U) {
    print("  -> iwdg\r\n");
    hal_uart_flush(USART1);
    (void)hal_iwdg_start(IWDG, 100U);
  } else {
    print("  -> software reset\r\n");
    hal_uart_flush(USART1);
    hal_delay_ms(500U); /* room to see each boot on the analyzer */
    hal_reset_software();
  }

  while (1) {
    /* no refresh: the watchdog resets us */
  }
}
