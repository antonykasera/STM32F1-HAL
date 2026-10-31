/**
 * @file main.c
 * @brief AS5600 magnetic encoder read over I2C1, printed on USART1
 *
 * Reads the STATUS register (magnet detected / weak / strong) once at start as
 * a physical-setup check, then streams RAW_ANGLE (0..4095, 12-bit).
 *
 * Wiring: AS5600 SCL -> PB6, SDA -> PB7, VCC -> 3.3V, GND -> GND (pull-ups
 * are 4.7k to 3.3V).Serial out on USART1 (PA9 TX), 115200 8N1.
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
#define AS5600_STATUS 0x0BU    /* MD (bit5) / ML (bit4) / MH (bit3) */
#define AS5600_RAW_ANGLE 0x0EU /* 0x0E high (11:8), 0x0F low (7:0) */

#define AS5600_MD (1U << 5)
#define AS5600_ML (1U << 4)
#define AS5600_MH (1U << 3)

#define I2C_PORT GPIOB
#define I2C_SCL 6U
#define I2C_SDA 7U

#define UART_TX 9U
#define UART_PORT GPIOA

void SystemInit(void) {}

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

  /* USART1 for output */
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  hal_afio_enable();
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  hal_uart_init(USART1, &uart_cfg);

  /* I2C1 on PB6/PB7, alternate-function open-drain */
  GPIO_Config i2c_cfg = {GPIO_MODE_AF_OD, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(I2C_PORT);
  hal_gpio_pin_init(I2C_PORT, I2C_SCL, &i2c_cfg);
  hal_gpio_pin_init(I2C_PORT, I2C_SDA, &i2c_cfg);

  I2C_Config cfg = {I2C_SPEED_STANDARD, 10U};
  hal_i2c_init(I2C1, &cfg);

  /* a 1-byte read (exercises the N=1 path) */
  uint8_t status = 0U;
  if (hal_i2c_read_reg(I2C1, AS5600_ADDR, AS5600_STATUS, &status, 1U) !=
      HAL_OK) {
    hal_uart_write_string(USART1, "AS5600 not responding\r\n");
  } else if (!(status & AS5600_MD)) {
    hal_uart_write_string(USART1, "no magnet detected\r\n");
  } else if (status & AS5600_ML) {
    hal_uart_write_string(USART1, "magnet too weak (move closer)\r\n");
  } else if (status & AS5600_MH) {
    hal_uart_write_string(USART1, "magnet too strong (move away)\r\n");
  } else {
    hal_uart_write_string(USART1, "magnet OK\r\n");
  }

  while (1) {
    /* RAW_ANGLE: a 2-byte read (exercises the N=2 path + repeated START) */
    uint8_t raw[2] = {0U, 0U};
    HAL_Status st =
        hal_i2c_read_reg(I2C1, AS5600_ADDR, AS5600_RAW_ANGLE, raw, 2U);
    if (st == HAL_OK) {
      uint16_t angle = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]) & 0x0FFFU;
      hal_uart_write_string(USART1, "angle: ");
      uart_print_dec(angle);
      hal_uart_write_string(USART1, "\r\n");
    } else {
      /* report the specific fault: NACK vs TIMEOUT */
      hal_uart_write_string(USART1, "read error: ");
      hal_uart_write_string(USART1, hal_status_str(st));
      hal_uart_write_string(USART1, "\r\n");
    }

    hal_delay_ms(200U);
  }
}
