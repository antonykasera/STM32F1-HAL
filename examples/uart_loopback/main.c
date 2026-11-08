/**
 * @file main.c
 * @brief USART1 loopback: jumper TX (PA9) to RX (PA10), no adapter needed
 *
 * Checks try_read and flush against the chip's own echo, then prints on PA9
 * (decode it with the analyzer, 115200 8N1); the printout loops back too and
 * is drained. Press RESET to run again.
 *   empty         ok
 *   flush         ok
 *   256 bytes     ok
 *     sr 0xE8 kept 0x80
 *   overrun       ok
 *   4/4 passed
 * PB0 is high from the flush test's write until flush returns: ~87 us, ending
 * with the stop bit. A missing jumper fails everything but "empty".
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define UART_PORT GPIOA
#define UART_TX 9U
#define UART_RX 10U
#define MARK_PORT GPIOB
#define MARK_PIN 0U
#define BURST 20U

static uint32_t passed;
static uint32_t total;

void SystemInit(void) {}

static void print(const char *s) { hal_uart_write_string(USART1, s); }

/* drop whatever looped back, overrun included */
static void drain(void) {
  uint8_t b;
  hal_uart_flush(USART1);
  while (hal_uart_try_read_byte(USART1, &b) == HAL_OK) {
  }
}

static void print_hex8(uint8_t v) {
  const char hex[] = "0123456789ABCDEF";
  const char buf[] = {'0', 'x', hex[v >> 4], hex[v & 0xFU], '\0'};
  print(buf);
}

static void report(const char *name, bool ok) {
  total++;
  passed += ok ? 1U : 0U;
  print(name);
  print(ok ? "ok\r\n" : "FAIL\r\n");
  drain();
}

static bool echo_ok(uint8_t v) {
  uint8_t b = 0U;
  hal_uart_write_byte(USART1, v);
  hal_uart_flush(USART1);
  return (hal_uart_try_read_byte(USART1, &b) == HAL_OK) && (b == v);
}

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config rx_cfg = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  GPIO_Config mark_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50_MHZ,
                          GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_init_port_clock(MARK_PORT);
  hal_gpio_pin_init(MARK_PORT, MARK_PIN, &mark_cfg);
  hal_gpio_pin_write(MARK_PORT, MARK_PIN, false);
  hal_gpio_pin_init(UART_PORT, UART_RX, &rx_cfg);
  hal_uart_init(USART1, &uart_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);

  print("\r\nuart loopback selftest\r\n");
  drain();

  uint8_t b = 0U;
  report("empty         ",
         hal_uart_try_read_byte(USART1, &b) == HAL_ERR_EMPTY);

  /* TXE alone: still on the wire, nothing to read; after flush it has landed */
  hal_gpio_pin_write(MARK_PORT, MARK_PIN, true);
  hal_uart_write_byte(USART1, 0x55U);
  bool early = hal_uart_try_read_byte(USART1, &b) == HAL_ERR_EMPTY;
  hal_uart_flush(USART1);
  hal_gpio_pin_write(MARK_PORT, MARK_PIN, false);
  report("flush         ", early &&
                               (hal_uart_try_read_byte(USART1, &b) == HAL_OK) &&
                               (b == 0x55U));

  uint32_t good = 0U;
  for (uint32_t v = 0U; v < 256U; v++) {
    good += echo_ok((uint8_t)v) ? 1U : 0U;
  }
  report("256 bytes     ", good == 256U);

  /* BURST unread bytes overflow RX: ORE, only the first is kept (RM0008).
   * This clone has an 8-byte RX FIFO, but past 8 it keeps only the first too.
   * Then empty, and the next byte arrives normally */
  uint8_t kept[BURST] = {0};
  for (uint32_t k = 0U; k < BURST; k++) {
    hal_uart_write_byte(USART1, (uint8_t)(0x80U + k));
  }
  hal_uart_flush(USART1);
  uint32_t sr = USART1->SR;
  uint32_t n = 0U;
  while ((n < BURST) && (hal_uart_try_read_byte(USART1, &kept[n]) == HAL_OK)) {
    n++;
  }
  bool ok = ((sr & USART_SR_ORE) != 0U) && (n == 1U) && (kept[0] == 0x80U) &&
            echo_ok(0x5AU);
  print("  sr ");
  print_hex8((uint8_t)sr);
  print(" kept");
  for (uint32_t k = 0U; k < n; k++) {
    print(" ");
    print_hex8(kept[k]);
  }
  print("\r\n");
  report("overrun       ", ok);

  const char summary[] = {(char)('0' + passed), '/', (char)('0' + total), '\0'};
  print(summary);
  print(" passed\r\n");

  while (1) {
  }
}
