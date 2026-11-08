/**
 * @file main.c
 * @brief ADC example: PA0, VDDA (via VREFINT) and the die temperature
 *
 * Every 500 ms prints on USART1 (PA9, 115200), e.g.
 *   pa0=2047 1649mV vdda=3301mV temp=31.2C
 * Check: PA0 to GND reads near 0, to 3V3 near 4095, and vdda should match a
 * multimeter on 3V3 within the VREFINT spread (about +-3%).
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_adc.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_PORT GPIOA
#define UART_TX 9U
#define AIN_PORT GPIOA
#define AIN_PIN 0U
#define AIN_CH 0U

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

static void print_c10(int32_t c10) {
  if (c10 < 0) {
    print("-");
    c10 = -c10;
  }
  print_u32((uint32_t)c10 / 10U);
  print(".");
  print_u32((uint32_t)c10 % 10U);
}

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config ain_cfg = {GPIO_MODE_ANALOG, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_gpio_pin_init(AIN_PORT, AIN_PIN, &ain_cfg);
  hal_uart_init(USART1, &uart_cfg);

  if (hal_adc_init(ADC1) != HAL_OK) {
    print("adc init failed\r\n");
    for (;;) {
    }
  }

  while (1) {
    uint16_t ain = 0U;
    uint16_t vref = 0U;
    uint16_t temp = 0U;
    (void)hal_adc_read(ADC1, AIN_CH, ADC_SAMPLE_55_5, &ain);
    (void)hal_adc_read(ADC1, ADC_CH_VREFINT, ADC_SAMPLE_239_5, &vref);
    (void)hal_adc_read(ADC1, ADC_CH_TEMP, ADC_SAMPLE_239_5, &temp);

    uint32_t vdda = hal_adc_calc_vdda_mv(vref);

    print("pa0=");
    print_u32(ain);
    print(" ");
    print_u32(hal_adc_calc_mv(ain, vdda));
    print("mV vdda=");
    print_u32(vdda);
    print("mV temp=");
    print_c10(hal_adc_calc_temp_c10(hal_adc_calc_mv(temp, vdda)));
    print("C\r\n");

    hal_delay_ms(500U);
  }
}
