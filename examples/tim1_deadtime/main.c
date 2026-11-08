/**
 * @file main.c
 * @brief TIM1 complementary PWM with dead time and a break input
 *
 * CH1 on PA8 and CH1N on PB13: 20 kHz, 50% duty, 1 us dead time. On a logic
 * analyzer both pins are low for ~1 us at every switch (never high together).
 * Break: jumper PB12 (BKIN, pull-up) to GND and both outputs drop low at once,
 * in hardware; remove the jumper and the loop re-arms them. Prints on USART1
 * (PA9, 115200).
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "hal_tim1.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define UART_TX 9U
#define BKIN_PIN 12U

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
  GPIO_Config bkin_cfg = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  TIM1_Config pwm_cfg = {20000U, 1000U, true};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_init_port_clock(GPIOB);
  hal_gpio_pin_init(GPIOA, UART_TX, &af_cfg);
  hal_gpio_pin_init(GPIOA, 8U, &af_cfg);  /* CH1 */
  hal_gpio_pin_init(GPIOB, 13U, &af_cfg); /* CH1N */
  hal_gpio_pin_init(GPIOB, BKIN_PIN, &bkin_cfg);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);

  uint8_t dtg = 0U;
  uint32_t dead_ns = 0U;
  (void)hal_tim1_calc_dtg(hal_clock_get_timer_clk_hz(TIM1), pwm_cfg.dead_ns,
                          &dtg, &dead_ns);

  if (hal_tim1_init(TIM1, &pwm_cfg) != HAL_OK) {
    print("tim1 init failed\r\n");
    for (;;) {
    }
  }
  hal_tim1_channel_init(TIM1, PWM_CH1, true);
  hal_pwm_set_duty_permille(TIM1, PWM_CH1, 500U);
  hal_tim1_start(TIM1);

  print("tim1: 20 kHz, dead time ");
  print_u32(dead_ns);
  print(" ns\r\n");

  bool off = false;

  while (1) {
    if (hal_tim1_tripped(TIM1) && !off) {
      off = true;
      print("break: outputs off\r\n");
    }
    if (off && hal_gpio_pin_read(GPIOB, BKIN_PIN)) {
      hal_tim1_start(TIM1); /* BKIN released: re-arm */
      off = false;
      print("re-armed\r\n");
    }
    hal_delay_ms(100U);
  }
}
