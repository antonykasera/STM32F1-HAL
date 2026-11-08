/**
 * @file main.c
 * @brief Pulse measurement self-test: TIM3 PWM measured by TIM2, one jumper
 *
 * Jumper PA6 (TIM3 CH1, PWM out) -> PA0 (TIM2 CH1, capture in). TIM3 makes a
 * 1 kHz PWM whose duty steps 25 -> 50 -> 75% each second; TIM2 measures it in
 * 1 us ticks. Prints on USART1 (PA9, 115200), e.g.
 *   period 1000 us, high 250 us (want 1000 / 250)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_capture.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_TX 9U

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
  GPIO_Config af_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config in_cfg = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  static const uint16_t duty[] = {250U, 500U, 750U};
  Capture cap;

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_pin_init(GPIOA, UART_TX, &af_cfg);
  hal_gpio_pin_init(GPIOA, 6U, &af_cfg); /* TIM3 CH1 out */
  hal_gpio_pin_init(GPIOA, 0U, &in_cfg); /* TIM2 CH1 in */
  hal_uart_init(USART1, &uart_cfg);

  hal_pwm_init(TIM3, 1000U);
  hal_pwm_channel_init(TIM3, PWM_CH1);
  hal_pwm_start(TIM3);
  (void)hal_capture_init(&cap, TIM2, 1000000U);

  uint32_t step = 0U;

  while (1) {
    hal_pwm_set_duty_permille(TIM3, PWM_CH1, duty[step]);
    hal_delay_ms(1000U); /* let several periods pass at the new duty */

    uint32_t period = 0U;
    uint32_t high = 0U;
    if (hal_capture_try_read(&cap, &period, &high) == HAL_OK) {
      print("period ");
      print_u32(period);
      print(" us, high ");
      print_u32(high);
      print(" us (want 1000 / ");
      print_u32(duty[step]);
      print(")\r\n");
    } else {
      print("no signal\r\n");
    }

    step = (step + 1U) % 3U;
  }
}
