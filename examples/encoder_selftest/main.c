/**
 * @file main.c
 * @brief Encoder self-test: software quadrature into TIM2, no encoder needed
 *
 * Jumpers PB0 - PA0 (A, TIM2 CH1) and PB1 - PA1 (B, TIM2 CH2). Once a second
 * the loop steps a quadrature pattern 100 edges forward (A leading B) then 30
 * back, and checks the encoder moved by exactly +70. Prints on USART1 (PA9,
 * 115200), e.g.
 *   moved 70 (want 70) ok, position 140
 * Swap the jumpers and every move should read -70 instead.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_encoder.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_TX 9U
#define FWD 100
#define BACK 30

void SystemInit(void) {}

/* bit0 = A, bit1 = B: 00 -> 10 -> 11 -> 01 has A leading B */
static const uint8_t quad[4] = {0U, 1U, 3U, 2U};
static uint32_t phase;

static void step(int32_t dir) {
  phase = (phase + ((dir > 0) ? 1U : 3U)) & 3U;
  hal_gpio_pin_write(GPIOB, 0U, (quad[phase] & 1U) != 0U);
  hal_gpio_pin_write(GPIOB, 1U, (quad[phase] & 2U) != 0U);
  for (volatile uint32_t d = 0U; d < 50U; d++) {
  }
}

static void print(const char *s) { hal_uart_write_string(USART1, s); }

static void print_i32(int32_t v) {
  char buf[12];
  uint32_t i = 11U;
  uint32_t u = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
  buf[i] = '\0';
  do {
    buf[--i] = (char)('0' + (u % 10U));
    u /= 10U;
  } while (u > 0U);
  if (v < 0) {
    buf[--i] = '-';
  }
  print(&buf[i]);
}

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config in_cfg = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  GPIO_Config out_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  Encoder enc;

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_init_port_clock(GPIOB);
  hal_gpio_pin_init(GPIOA, UART_TX, &tx_cfg);
  hal_gpio_pin_init(GPIOA, 0U, &in_cfg);
  hal_gpio_pin_init(GPIOA, 1U, &in_cfg);
  hal_gpio_pin_init(GPIOB, 0U, &out_cfg);
  hal_gpio_pin_init(GPIOB, 1U, &out_cfg);
  hal_gpio_pin_write(GPIOB, 0U, false);
  hal_gpio_pin_write(GPIOB, 1U, false);
  hal_uart_init(USART1, &uart_cfg);

  (void)hal_encoder_init(&enc, TIM2, 0U, false); /* clean edges: no filter */

  while (1) {
    int32_t before = hal_encoder_update(&enc);
    for (int32_t i = 0; i < FWD; i++) {
      step(1);
    }
    for (int32_t i = 0; i < BACK; i++) {
      step(-1);
    }
    int32_t after = hal_encoder_update(&enc);
    int32_t moved = after - before;

    print("moved ");
    print_i32(moved);
    print(" (want ");
    print_i32(FWD - BACK);
    print((moved == (FWD - BACK)) ? ") ok, position " : ") FAIL, position ");
    print_i32(after);
    print("\r\n");

    hal_delay_ms(1000U);
  }
}
