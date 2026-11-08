/**
 * @file main.c
 * @brief AS5600 RAW_ANGLE via interrupt-driven I2C.
 *
 * Kicks off hal_i2c_read_reg_it and returns immediately; the completion
 * callback fires from the I2C ISR with the result. The superloop toggles PC13
 * while it waits to show the CPU is NOT blocked during the transfer, and gives
 * up after READ_TIMEOUT_MS with hal_i2c_abort_it.
 *
 *   S · AW:36 · DW:0C · Sr · AR:36 · DR · DR(N) · P
 *
 * Wiring: SCL=PB6, SDA=PB7, pull-ups to 3.3V. UART TX on PA9.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdint.h>

#define AS5600_ADDR 0x36U
#define AS5600_RAW_ANGLE 0x0CU /* 0x0C high (11:8), 0x0D low */

#define I2C_PORT GPIOB
#define I2C_SCL 6U
#define I2C_SDA 7U
#define UART_TX 9U
#define UART_PORT GPIOA
#define LED_PORT GPIOC
#define LED_PIN 13U
#define READ_TIMEOUT_MS 10U

static volatile uint8_t g_raw[2];
static volatile bool g_done;
static volatile HAL_Status g_result;

void SystemInit(void) {}

/* fired from the I2C ISR when the transfer completes */
static void on_i2c_done(I2C_TypeDef *i2c, HAL_Status status) {
  (void)i2c;
  g_result = status;
  g_done = true;
}

static void uart_print_dec(uint16_t v) {
  char buf[6];
  int i = 5;
  buf[i--] = '\0';
  if (v == 0U) {
    buf[i--] = '0';
  }
  while (v > 0U) {
    buf[i--] = (char)('0' + (v % 10U));
    v /= 10U;
  }
  hal_uart_write_string(USART1, &buf[i + 1]);
}

int main(void) {
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  /* UART enabled before PA9 becomes AF, so TX starts idle-high */
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  hal_gpio_init_port_clock(UART_PORT);
  hal_afio_enable();
  hal_uart_init(USART1, &uart_cfg);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);

  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);

  /* a reset mid-transfer can leave the sensor holding SDA low: clock it free
   * (this also runs hal_i2c_init) */
  I2C_Config cfg = {I2C_SPEED_STANDARD, 10U};
  hal_gpio_init_port_clock(I2C_PORT);
  (void)hal_i2c_bus_recover(I2C1, I2C_PORT, I2C_SCL, I2C_SDA, &cfg);
  hal_i2c_register_callback(I2C1, on_i2c_done);

  while (1) {
    g_done = false;
    HAL_Status st = hal_i2c_read_reg_it(I2C1, AS5600_ADDR, AS5600_RAW_ANGLE,
                                        (uint8_t *)g_raw, 2U);
    if (st != HAL_OK) {
      hal_uart_write_string(USART1, "start error: ");
      hal_uart_write_string(USART1, hal_status_str(st));
      hal_uart_write_string(USART1, "\r\n");
      (void)hal_i2c_bus_recover(I2C1, I2C_PORT, I2C_SCL, I2C_SDA, &cfg);
      hal_delay_ms(200U);
      continue;
    }

    /* CPU is free while the transfer runs */
    uint32_t start = hal_get_tick();
    while (!g_done) {
      hal_gpio_pin_toggle(LED_PORT, LED_PIN);
      if (hal_tick_expired(hal_get_tick(), start, READ_TIMEOUT_MS)) {
        hal_i2c_abort_it(I2C1); /* calls back with HAL_ERR_TIMEOUT */
      }
    }

    if (g_result == HAL_OK) {
      uint16_t angle =
          (uint16_t)(((uint16_t)g_raw[0] << 8) | g_raw[1]) & 0x0FFFU;
      hal_uart_write_string(USART1, "angle: ");
      uart_print_dec(angle);
      hal_uart_write_string(USART1, "\r\n");
    } else {
      hal_uart_write_string(USART1, "read error: ");
      hal_uart_write_string(USART1, hal_status_str(g_result));
      hal_uart_write_string(USART1, "\r\n");
    }

    hal_delay_ms(200U);
  }
}
