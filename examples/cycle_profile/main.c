/**
 * @file main.c
 * @brief Cycle-counting profiler example.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * Uses the Cortex-M3 DWT->CYCCNT free-running cycle counter to measure how many
 * CPU cycles a broad cross-section of HAL calls take, and prints the results
 * over USART1 (PA9 TX, 115200 8N1).  Read with: picocom -b 115200 /dev/ttyUSB0
 *
 * Notes:
 *  - CYCCNT increments once per core cycle (72 MHz -> 13.9 ns/cycle here).
 *  - "read overhead" is the cost of the two CYCCNT reads themselves; subtract
 *    it from the small measurements for the true figure.
 *  - Pure functions are fed VOLATILE inputs so the optimiser cannot
 *    constant-fold them at build time (which would report a misleading ~4).
 *  - Compare an -O0 build (what the code does) against -O2 -flto (what actually
 *    runs). Blocking I/O (uart_write_byte) is dominated by hardware wait, not
 *    compute, so it barely changes between the two.
 */

#include "hal_clock.h"
#include "hal_exti.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define UART_TX 9U
#define UART_RX 10U
#define UART_PORT GPIOA

#define LED_PORT GPIOC
#define LED_PIN 13U

void SystemInit(void) {}

/* DWT cycle counter */

static void cycles_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t cycles_now(void) { return DWT->CYCCNT; }

static void uart_print(const char *s) { hal_uart_write_string(USART1, s); }

static void uart_print_u32(uint32_t v) {
  char buf[11];
  uint8_t i = 10U;
  buf[i] = '\0';
  if (v == 0U) {
    buf[--i] = '0';
  }
  while (v > 0U) {
    buf[--i] = (char)('0' + (v % 10U));
    v /= 10U;
  }
  uart_print(&buf[i]);
}

static void report(const char *label, uint32_t cyc) {
  uart_print(label);
  uart_print(": ");
  uart_print_u32(cyc);
  uart_print(" cyc\r\n");
}

/* time a single statement and print the cycle delta */
#define MEASURE(label, stmt)                                                   \
  do {                                                                         \
    uint32_t _a = cycles_now();                                                \
    stmt;                                                                      \
    uint32_t _b = cycles_now();                                                \
    report(label, _b - _a);                                                    \
  } while (0)

int main(void) {
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  GPIO_Config rx_cfg = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();
  cycles_init();

  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(UART_PORT, UART_RX, &rx_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_uart_init(USART1, &uart_cfg);

  /* stop the optimiser from precomputing pure functions */
  volatile uint8_t v_pin = LED_PIN;
  volatile uint8_t v_level = 1U;
  volatile GPIO_Mode v_mode = GPIO_MODE_AF_PP;
  volatile GPIO_Speed v_speed = GPIO_SPEED_50_MHZ;
  volatile GPIO_Pull v_pull = GPIO_PULL_NONE;
  volatile uint32_t v_pclk = 72000000U;
  volatile uint32_t v_baud = 115200U;
  volatile uint32_t v_freq = 1000U;
  volatile float v_fa = 3.14159f;
  volatile float v_fb = 2.71828f;

  /* sinks so results are "used" and not dead-code-eliminated */
  volatile uint8_t sink_u8 = 0U;
  volatile uint16_t sink_u16 = 0U;
  volatile bool sink_b = false;
  volatile float sink_f = 0.0f;

  while (1) {
    uart_print("\r\ncycle profile @72MHz (13.9 ns/cyc)\r\n");

    /* baseline */
    MEASURE("read overhead", (void)0);

    /* GPIO single-register ops */
    MEASURE("gpio_pin_write", hal_gpio_pin_write(LED_PORT, v_pin, v_level));
    MEASURE("gpio_pin_toggle", hal_gpio_pin_toggle(LED_PORT, v_pin));
    MEASURE("gpio_pin_read", sink_b = hal_gpio_pin_read(LED_PORT, v_pin));
    MEASURE("gpio_port_write", hal_gpio_port_write(LED_PORT, 0U));
    MEASURE("gpio_port_read", sink_u16 = hal_gpio_port_read(LED_PORT));
    MEASURE("gpio_cr_shift", sink_u8 = gpio_cr_shift(v_pin));

    /* GPIO: full pin init (register RMW + encode) */
    MEASURE("gpio_pin_init", hal_gpio_pin_init(LED_PORT, v_pin, &led_cfg));

    /* pure encoders*/
    MEASURE("gpio_encode_config",
            sink_u8 = gpio_encode_config(v_mode, v_speed, v_pull));

    /* UART baud math */
    MEASURE("uart_calc_brr", sink_u16 = hal_uart_calc_brr(v_pclk, v_baud));

    /* timer PSC/ARR solver: divide + loop */
    {
      uint16_t psc = 0U;
      uint16_t arr = 0U;
      MEASURE("timer_solve_psc_arr",
              hal_timer_solve_psc_arr(v_pclk, v_freq, &psc, &arr));
      sink_u16 = (uint16_t)(psc + arr);
    }

    /* clock query, returns a stored value */
    MEASURE("clock_get_pclk2", sink_u16 = (uint16_t)hal_clock_get_pclk2_hz());

    /* soft-float tax on a no-FPU */
    MEASURE("float multiply", sink_f = v_fa * v_fb);
    MEASURE("float divide", sink_f = v_fa / v_fb);
    MEASURE("uint32 divide", sink_u16 = (uint16_t)(v_pclk / v_baud));

    /* blocking UART TX */
    MEASURE("uart_write_byte", hal_uart_write_byte(USART1, '.'));

    /* max GPIO toggle rate */
    {
      const uint32_t bset = (1U << LED_PIN); /* BSRR: drive high */
      const uint32_t brst = (1U << LED_PIN); /* BRR:  drive low  */
      const uint32_t LOOPS = 1000U;          /* x8 pairs => 8000 toggles */
      const uint32_t TOGGLES = LOOPS * 8U;
      GPIO_TypeDef *g = LED_PORT;

      uint32_t a = cycles_now();
      for (uint32_t i = 0U; i < LOOPS; i++) {
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
        g->BSRR = bset;
        g->BRR = brst;
      }
      uint32_t b = cycles_now();
      uint32_t total = b - a;

      /* cycles per full toggle (high+low), scaled x100 for 2-dp without float
       */
      uint32_t cyc_per_toggle_x100 = (total * 100U) / TOGGLES;
      /* implied square-wave freq in kHz: 72000 kHz / cycles_per_toggle */
      uint32_t freq_khz = (72000U * TOGGLES) / total;

      uart_print("toggle: ");
      uart_print_u32(total);
      uart_print(" cyc / ");
      uart_print_u32(TOGGLES);
      uart_print(" toggles\r\n");
      uart_print("  cyc/toggle x100: ");
      uart_print_u32(cyc_per_toggle_x100);
      uart_print("\r\n  implied sq-wave: ");
      uart_print_u32(freq_khz);
      uart_print(" kHz\r\n");
    }

    (void)sink_u8;
    (void)sink_u16;
    (void)sink_b;
    (void)sink_f;

    hal_delay_ms(2000U);
  }
}
