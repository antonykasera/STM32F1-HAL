/**
 * @file main.c
 * @brief ADC scan by DMA: four channels refreshed in the background
 *
 * ADC1 converts PA0, PA1, VREFINT and the temp sensor over and over; DMA ch1
 * keeps the latest four results in `latest[]`. The loop only reads that array,
 * every 500 ms, and prints on USART1 (PA9, 115200), e.g.
 *   pa0=3235mV pa1=0mV vdda=3235mV temp=23.0C
 * Move jumpers between GND and 3V3 on PA0/PA1 and watch both follow.
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

void SystemInit(void) {}

static const uint8_t channels[] = {0U, 1U, ADC_CH_VREFINT, ADC_CH_TEMP};
static volatile uint16_t latest[4]; /* written by DMA */

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
  hal_gpio_pin_init(GPIOA, 0U, &ain_cfg);
  hal_gpio_pin_init(GPIOA, 1U, &ain_cfg);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);

  /* 239.5 cycles suits all four, as the internal channels need it */
  if ((hal_adc_init(ADC1) != HAL_OK) ||
      (hal_adc_scan_start(ADC1, channels, 4U, ADC_SAMPLE_239_5, latest) !=
       HAL_OK)) {
    print("adc scan failed to start\r\n");
    for (;;) {
    }
  }

  while (1) {
    uint32_t vdda = hal_adc_calc_vdda_mv(latest[2]);

    print("pa0=");
    print_u32(hal_adc_calc_mv(latest[0], vdda));
    print("mV pa1=");
    print_u32(hal_adc_calc_mv(latest[1], vdda));
    print("mV vdda=");
    print_u32(vdda);
    print("mV temp=");
    print_c10(hal_adc_calc_temp_c10(hal_adc_calc_mv(latest[3], vdda)));
    print("C\r\n");

    hal_delay_ms(500U);
  }
}
