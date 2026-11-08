/**
 * @file main.c
 * @brief Flash + CRC: a boot counter kept in the last flash page
 *
 * Each boot checks the CRC unit against the software model, reads a
 * CRC-checked record from the last page, then erases the page and writes the
 * count + 1. Press reset to count up. USART1 (PA9, 115200), e.g.
 *   crc unit 0xDF8A8A2B, software 0xDF8A8A2B
 *   record ok: 2 boots
 *   saved boot 3: HAL_OK, read back ok
 * PC13 is high during the erase, then again during the write, so a logic
 * analyzer shows their real length (datasheet: 20-40 ms, 40-70 us per
 * half-word). st-flash write leaves the page alone; st-flash erase clears it.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_crc.h"
#include "hal_error.h"
#include "hal_flash.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define LED_PORT GPIOC
#define LED_PIN 13U
#define UART_TX 9U
#define RECORD_ADDR FLASH_LAST_PAGE
#define RECORD_MAGIC 0xF1E40001U

typedef struct {
  uint32_t magic;
  uint32_t boots;
  uint32_t crc; /* over magic and boots */
} Record;

void SystemInit(void) {}

static void print(const char *s) { hal_uart_write_string(USART1, s); }

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

static void print_hex(uint32_t v) {
  char buf[11] = "0x";
  for (uint32_t i = 0U; i < 8U; i++) {
    uint32_t nib = (v >> (28U - (4U * i))) & 0xFU;
    buf[2U + i] = (char)((nib < 10U) ? ('0' + nib) : ('A' + nib - 10U));
  }
  buf[10] = '\0';
  print(buf);
}

static uint32_t record_crc(const Record *r) {
  const uint32_t words[2] = {r->magic, r->boots};
  hal_crc_reset(CRC);
  return hal_crc_feed(CRC, words, 2U);
}

static bool record_ok(const Record *r) {
  return (r->magic == RECORD_MAGIC) && (r->crc == record_crc(r));
}

static void marker(bool high) { hal_gpio_pin_write(LED_PORT, LED_PIN, high); }

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(GPIOA, UART_TX, &tx_cfg);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  marker(false);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);
  hal_crc_init();

  const uint32_t probe = 0x12345678U;
  hal_crc_reset(CRC);
  print("crc unit ");
  print_hex(hal_crc_feed(CRC, &probe, 1U));
  print(", software ");
  print_hex(hal_crc_calc(HAL_CRC_INIT, &probe, 1U));
  print("\r\n");

  const Record *stored = (const Record *)RECORD_ADDR;
  Record next = {RECORD_MAGIC, 1U, 0U};
  if (record_ok(stored)) {
    next.boots = stored->boots + 1U;
    print("record ok: ");
    print_u32(stored->boots);
    print(" boots\r\n");
  } else {
    print("no valid record (first run or page erased)\r\n");
  }
  next.crc = record_crc(&next);

  marker(true);
  HAL_Status st = hal_flash_erase_page(FLASH, RECORD_ADDR);
  marker(false);
  if (st == HAL_OK) {
    marker(true);
    st = hal_flash_write(FLASH, (void *)RECORD_ADDR, &next, sizeof next);
    marker(false);
  }

  print("saved boot ");
  print_u32(next.boots);
  print(": ");
  print(hal_status_str(st));
  print((record_ok(stored) && (stored->boots == next.boots))
            ? ", read back ok\r\n"
            : ", read back FAILED\r\n");

  while (1) {
  }
}
