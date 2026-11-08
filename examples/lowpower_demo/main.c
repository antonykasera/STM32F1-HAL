/**
 * @file main.c
 * @brief Low power: three 2 s Stop naps woken by the RTC, then 5 s of Standby
 *
 * USART1 (PA9, 115200) shows the cycle, e.g.
 *   boot: power-on, rtc on LSE
 *   rtc 0: stop for 2 s ... woke at rtc 2
 *   ...
 *   standby for 5 s (cycle 1)
 *   boot: woke from standby, cycle 1, rtc 11
 * PC13 LED is on while awake. The cycle count lives in backup register DR1,
 * which survives Standby while RAM does not.
 *
 * KEEP_DEBUG 1 keeps the ST-Link connected (and reflashing easy) through Stop
 * and Standby but leaves clocks running; set 0 to measure real current.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_pwr.h"
#include "hal_rtc.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define KEEP_DEBUG 1
#define LED_PORT GPIOC
#define LED_PIN 13U
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

static void led(bool on) { hal_gpio_pin_write(LED_PORT, LED_PIN, !on); }

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();
  hal_pwr_debug_keepalive(KEEP_DEBUG != 0);

  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(GPIOA, UART_TX, &tx_cfg);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);
  led(true);

  bool from_standby = hal_pwr_woke_from_standby();
  const char *clk = "LSE";
  if (hal_rtc_init(RTC, RTC_CLOCK_LSE, 3000U) != HAL_OK) {
    clk = "LSI (no LSE crystal?)";
    (void)hal_rtc_init(RTC, RTC_CLOCK_LSI, 100U);
  }
  hal_rtc_alarm_wakes_stop();

  if (from_standby) {
    print("boot: woke from standby, cycle ");
    print_u32(hal_rtc_backup_read(1U));
    print(", rtc ");
    print_u32(hal_rtc_get(RTC));
    print("\r\n");
  } else {
    hal_rtc_backup_write(1U, 0U);
    print("boot: power-on, rtc on ");
    print(clk);
    print("\r\n");
  }

  for (uint32_t nap = 0U; nap < 3U; nap++) {
    uint32_t now = hal_rtc_get(RTC);
    print("rtc ");
    print_u32(now);
    print(": stop for 2 s ... ");
    hal_uart_flush(USART1); /* the UART stops with the clocks */

    (void)hal_rtc_alarm_at(RTC, now + 2U);
    led(false);
    hal_pwr_stop(&CLOCK_CONFIG_72MHZ_HSE);
    led(true);

    print("woke at rtc ");
    print_u32(hal_rtc_get(RTC));
    print(hal_rtc_alarm_fired(RTC) ? " (alarm)\r\n" : " (other wake-up)\r\n");
  }

  uint16_t cycle = (uint16_t)(hal_rtc_backup_read(1U) + 1U);
  hal_rtc_backup_write(1U, cycle);
  print("standby for 5 s (cycle ");
  print_u32(cycle);
  print(")\r\n");
  hal_uart_flush(USART1);

  (void)hal_rtc_alarm_at(RTC, hal_rtc_get(RTC) + 5U);
  led(false);
  hal_pwr_standby(); /* wakes as a reset, back at the top of main */
}
