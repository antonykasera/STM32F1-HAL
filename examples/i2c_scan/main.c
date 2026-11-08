/**
 * @file main.c
 * @brief I2C bus scanner: probe 0x08..0x77, report ACKing addresses over UART
 *
 * Wiring: SCL = PB6, SDA = PB7, with pull-ups to 3.3V. Serial output on USART1
 * (PA9 TX), 115200 8N1.
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

#define I2C_PORT GPIOB
#define I2C_SCL 6U
#define I2C_SDA 7U

#define UART_TX 9U
#define UART_PORT GPIOA

void SystemInit(void) {}

static void uart_print_hex(uint8_t v) {
  static const char digits[] = "0123456789ABCDEF";
  char out[5] = {'0', 'x', digits[v >> 4], digits[v & 0x0FU], '\0'};
  hal_uart_write_string(USART1, out);
}

int main(void) {
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  /* USART1 for output */
  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(UART_PORT);
  hal_gpio_pin_init(UART_PORT, UART_TX, &tx_cfg);
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  hal_uart_init(USART1, &uart_cfg);

  /* I2C1 on PB6/PB7, alternate-function open-drain */
  GPIO_Config i2c_cfg = {GPIO_MODE_AF_OD, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(I2C_PORT);
  hal_gpio_pin_init(I2C_PORT, I2C_SCL, &i2c_cfg);
  hal_gpio_pin_init(I2C_PORT, I2C_SDA, &i2c_cfg);

  I2C_Config cfg = {I2C_SPEED_STANDARD, 10U};
  hal_i2c_init(I2C1, &cfg);

  while (1) {
    hal_uart_write_string(USART1, "Scanning...\r\n");

    for (uint8_t addr = 0x08U; addr <= 0x77U; addr++) {
      if (hal_i2c_probe(I2C1, addr) == HAL_OK) {
        hal_uart_write_string(USART1, "  found ");
        uart_print_hex(addr);
        hal_uart_write_string(USART1, "\r\n");
      }
    }

    hal_uart_write_string(USART1, "done\r\n");
    hal_delay_ms(2000U);
  }
}
