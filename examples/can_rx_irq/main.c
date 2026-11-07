/**
 * @file main.c
 * @brief CAN RX queue demo: a burst survives a slow superloop
 *
 * Loopback, 500 kbit/s, PB8/PB9. Once a second: send 10 frames back to back,
 * stay "busy" for 20 ms without reading, then drain and report on USART1 (PA9,
 * 115200). The hardware FIFO holds 3 frames, so with RX_IRQ 0 expect "got 3/10
 * lost=yes". With RX_IRQ 1 the interrupt moves each frame into a 16-deep RAM
 * queue as it arrives: expect "got 10/10 lost=no".
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_can.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define RX_IRQ 1

#define UART_PORT GPIOA
#define UART_TX 9U
#define CAN_PORT GPIOB
#define CAN_RX 8U
#define CAN_TX 9U
#define BURST 10U
#define BUSY_MS 20U

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
  GPIO_Config can_rx = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  CAN_Config can_cfg = {500000U, 0U, CAN_MODE_LOOPBACK, 10U};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &uart_tx);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);

  hal_gpio_init_port_clock(CAN_PORT);
  hal_gpio_pin_init(CAN_PORT, CAN_RX, &can_rx);
  hal_gpio_pin_init(CAN_PORT, CAN_TX, &can_tx);
  hal_afio_remap(AFIO_MAPR_CAN_REMAP, AFIO_MAPR_CAN_REMAP_REMAP2);

  if (hal_can_init(CAN1, &can_cfg) != HAL_OK) {
    print("can init failed\r\n");
    for (;;) {
    }
  }
#if RX_IRQ
  hal_can_enable_rx_interrupt(CAN1, 5U);
  print("rx: interrupt + queue\r\n");
#else
  print("rx: polled\r\n");
#endif

  while (1) {
    for (uint32_t sent = 0U; sent < BURST;) {
      CAN_Frame tx = {0x123U, false, false, 1U, {(uint8_t)sent}};
      if (hal_can_try_send(CAN1, &tx) == HAL_OK) {
        sent++;
      }
    }

    hal_delay_ms(BUSY_MS); /* stand-in for slow work in the loop */

    uint32_t got = 0U;
    CAN_Frame rx;
    while (hal_can_try_receive(CAN1, 0U, &rx) == HAL_OK) {
      got++;
    }

    CAN_Errors e;
    hal_can_get_errors(CAN1, &e);
    print("got ");
    print_u32(got);
    print("/10 lost=");
    print(e.rx_overrun ? "yes\r\n" : "no\r\n");

    hal_delay_ms(1000U);
  }
}
