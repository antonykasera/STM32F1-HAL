/**
 * @file main.c
 * @brief CAN loopback self-test on PB8 (RX) / PB9 (TX), no transceiver needed
 *
 * Loopback feeds TX back to RX inside the chip, so every frame sent comes
 * straight back. Every 100 ms: send id 0x123 carrying a counter, read it back,
 * report on USART1 (PA9, 115200). Probe PB9 with a CAN decoder at 500 kbit/s;
 * the ACK slot stays recessive (no other node), which loopback ignores.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_can.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_PORT GPIOA
#define UART_TX 9U
#define CAN_PORT GPIOB
#define CAN_RX 8U
#define CAN_TX 9U
#define FRAME_ID 0x123U
#define PERIOD_MS 100U

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

int main(void) {
  GPIO_Config uart_tx = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config can_tx = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  /* pull-up idles RX recessive with no transceiver; use plain input for a 5V
   * transceiver such as MCP2551 (pulls must be off above VDD + 0.3 V) */
  GPIO_Config can_rx = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  CAN_Config can_cfg = {500000U, 0U, CAN_MODE_LOOPBACK, 10U};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &uart_tx);
  hal_uart_init(USART1, &uart_cfg);

  /* PA11/PA12 go to USB on the Blue Pill, so use the PB8/PB9 remap */
  hal_gpio_init_port_clock(CAN_PORT);
  hal_gpio_pin_init(CAN_PORT, CAN_RX, &can_rx);
  hal_gpio_pin_init(CAN_PORT, CAN_TX, &can_tx);
  hal_afio_remap(AFIO_MAPR_CAN_REMAP, AFIO_MAPR_CAN_REMAP_REMAP2);

  HAL_Status st = hal_can_init(CAN1, &can_cfg);
  if (st != HAL_OK) {
    print("can init failed: ");
    print(hal_status_str(st));
    print("\r\n");
    for (;;) {
    }
  }
  print("can loopback up, 500 kbit/s\r\n");

  uint32_t count = 0U;
  uint32_t last_send = hal_get_tick();

  while (1) {
    if (hal_tick_expired(hal_get_tick(), last_send, PERIOD_MS)) {
      last_send = hal_get_tick();
      CAN_Frame tx = {FRAME_ID, false, false, 4U, {0}};
      tx.data[0] = (uint8_t)count;
      tx.data[1] = (uint8_t)(count >> 8);
      tx.data[2] = (uint8_t)(count >> 16);
      tx.data[3] = (uint8_t)(count >> 24);
      if (hal_can_try_send(CAN1, &tx) == HAL_OK) {
        count++;
      } else {
        print("tx busy\r\n");
      }
    }

    CAN_Frame rx;
    if (hal_can_try_receive(CAN1, 0U, &rx) == HAL_OK) {
      uint32_t n = (uint32_t)rx.data[0] | ((uint32_t)rx.data[1] << 8) |
                   ((uint32_t)rx.data[2] << 16) | ((uint32_t)rx.data[3] << 24);
      print((rx.id == FRAME_ID) ? "rx ok n=" : "rx WRONG ID n=");
      print_u32(n);
      print("\r\n");
    }
  }
}
