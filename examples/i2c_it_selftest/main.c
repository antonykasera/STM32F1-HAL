/**
 * @file main.c
 * @brief Interrupt-mode I2C bench test against an AS5600, checked by blocking
 *
 * Each read runs twice, blocking (the reference) and interrupt-driven, and
 * must match, finish with HAL_OK and leave the canary byte after the buffer
 * alone. USART1 (PA9, 115200) prints one line per case, then a summary:
 *   N=1 status    ok
 *   ...
 *   7/7 passed
 * On the analyzer each read must end with a NACK on its last byte, then P.
 * Press RESET to run again (no power-cycle needed: the bus is recovered first).
 *
 * Wiring: SCL=PB6, SDA=PB7, pull-ups to 3.3V. Nothing at 0x37 (NACK case).
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_uart.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define AS5600_ADDR 0x36U
#define ABSENT_ADDR 0x37U
#define REG_ZMCO 0x00U /* 0x00-0x04: ZMCO, ZPOS, MPOS: stable values */
#define REG_STATUS 0x0BU
#define REG_RAW_ANGLE 0x0CU
#define CANARY 0xA5U
#define IT_TIMEOUT_MS 10U

#define I2C_PORT GPIOB
#define I2C_SCL 6U
#define I2C_SDA 7U

static volatile bool done;
static volatile HAL_Status result;
static uint32_t passed;
static uint32_t total;

void SystemInit(void) {}

static void on_done(I2C_TypeDef *i2c, HAL_Status st) {
  (void)i2c;
  result = st;
  done = true;
}

static HAL_Status wait_it(void) {
  uint32_t start = hal_get_tick();
  while (!done) {
    if (hal_tick_expired(hal_get_tick(), start, IT_TIMEOUT_MS)) {
      hal_i2c_abort_it(I2C1);
    }
  }
  return result;
}

static void print(const char *s) { hal_uart_write_string(USART1, s); }

static void report(const char *name, bool ok, HAL_Status st) {
  total++;
  passed += ok ? 1U : 0U;
  print(name);
  print(ok ? "ok" : "FAIL ");
  if (!ok) {
    print(hal_status_str(st));
  }
  print("\r\n");
}

/* interrupt read vs blocking read of the same registers */
static void check_read(const char *name, uint8_t reg, uint32_t n) {
  uint8_t ref[6];
  uint8_t got[6];
  memset(ref, 0, sizeof ref);
  memset(got, CANARY, sizeof got);

  HAL_Status st = hal_i2c_read_reg(I2C1, AS5600_ADDR, reg, ref, n);
  if (st == HAL_OK) {
    done = false;
    st = hal_i2c_read_reg_it(I2C1, AS5600_ADDR, reg, got, n);
    if (st == HAL_OK) {
      st = wait_it();
    }
  }
  report(name,
         (st == HAL_OK) && (memcmp(ref, got, n) == 0) && (got[n] == CANARY),
         st);
}

int main(void) {
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE);
  hal_systick_init();

  GPIO_Config tx_cfg = {GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  UART_Config uart_cfg = {115200, UART_PARITY_NONE, UART_STOP_1};
  hal_gpio_init_port_clock(GPIOA);
  hal_uart_init(USART1, &uart_cfg);
  hal_gpio_pin_init(GPIOA, 9U, &tx_cfg);

  I2C_Config cfg = {I2C_SPEED_STANDARD, 10U};
  hal_gpio_init_port_clock(I2C_PORT);
  (void)hal_i2c_bus_recover(I2C1, I2C_PORT, I2C_SCL, I2C_SDA, &cfg);
  hal_i2c_register_callback(I2C1, on_done);
  hal_delay_ms(20U); /* AS5600 power-up, if it came up with us */

  print("\r\ni2c interrupt selftest\r\n");

  check_read("N=1 status    ", REG_STATUS, 1U);
  check_read("N=2 zpos      ", REG_ZMCO + 1U, 2U);
  check_read("N=3 zmco..    ", REG_ZMCO, 3U);
  check_read("N=5 zmco..    ", REG_ZMCO, 5U);

  /* write: just the register pointer, harmless */
  const uint8_t ptr = REG_RAW_ANGLE;
  done = false;
  HAL_Status st = hal_i2c_write_it(I2C1, AS5600_ADDR, &ptr, 1U);
  report("write ptr     ", (st == HAL_OK) && (wait_it() == HAL_OK), result);

  /* absent device: the address must be NACKed, and the bus left usable */
  uint8_t dummy[2];
  done = false;
  st = hal_i2c_read_reg_it(I2C1, ABSENT_ADDR, REG_STATUS, dummy, 2U);
  st = (st == HAL_OK) ? wait_it() : st;
  report("nack 0x37     ", st == HAL_ERR_NACK, st);

  /* a second call while busy is refused and the first still completes */
  uint8_t status_it = 0U;
  uint8_t status_ref = 0U;
  (void)hal_i2c_read_reg(I2C1, AS5600_ADDR, REG_STATUS, &status_ref, 1U);
  done = false;
  st = hal_i2c_read_reg_it(I2C1, AS5600_ADDR, REG_STATUS, &status_it, 1U);
  HAL_Status second = hal_i2c_read_reg_it(I2C1, AS5600_ADDR, REG_RAW_ANGLE,
                                          dummy, 2U);
  st = (st == HAL_OK) ? wait_it() : st;
  report("busy refused  ",
         (second == HAL_ERR_BUSY) && (st == HAL_OK) &&
             (status_it == status_ref),
         (second != HAL_ERR_BUSY) ? second : st);

  const char summary[] = {(char)('0' + passed), '/', (char)('0' + total), '\0'};
  print(summary);
  print(" passed\r\n");

  while (1) {
  }
}
