/**
 * @file main.c
 * @brief PWM example: TIM3_CH1 on PA6, 1 kHz, breathing duty sweep
 *
 * Probe PA6 on a logic analyzer/scope: expect a 1 kHz square wave whose high
 * fraction (duty) ramps up and down. Wire an external LED (with a resistor) or
 * a scope to PA6 to see the effect.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define PWM_PORT GPIOA
#define PWM_PIN 6U
#define PWM_TIMER TIM3
#define PWM_CHANNEL PWM_CH1
#define PWM_FREQ_HZ 1000U

void SystemInit(void) {}

int main(void) {
  /* initialize the system clock and sysstem tick */
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  /* PA6 as alternate-function push-pull so the timer drives the pin. */
  hal_gpio_init_port_clock(PWM_PORT);

  GPIO_Config pwm_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_pin_init(PWM_PORT, PWM_PIN, &pwm_cfg);

  hal_pwm_init(PWM_TIMER, PWM_FREQ_HZ);
  hal_pwm_channel_init(PWM_TIMER, PWM_CHANNEL);
  hal_pwm_set_duty_permille(PWM_TIMER, PWM_CHANNEL, 250U); /* start at 25% */
  hal_pwm_start(PWM_TIMER);

  /* Breathing effect: sweep duty 0 to 100% to 0 in 1% steps.
   * Frequency stays 1 kHz throughout.
   */
  uint16_t duty = 0U;
  int16_t step = 10; /* 1.0% per tick */

  while (1) {
    hal_pwm_set_duty_permille(PWM_TIMER, PWM_CHANNEL, duty);

    if (duty >= 1000U) {
      step = -10;
    } else if (duty == 0U) {
      step = 10;
    }
    duty = (uint16_t)((int16_t)duty + step);

    hal_delay_ms(10U);
  }
}
