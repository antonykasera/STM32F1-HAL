/**
 * @file hal_i2c.h
 * @brief I2C master API header file
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { I2C_SPEED_STANDARD, I2C_SPEED_FAST } I2C_Speed;

typedef struct {
  I2C_Speed speed;
  uint32_t timeout_ms;
} I2C_Config;

typedef enum {
  I2C_STATE_READY,
  I2C_STATE_BUSY,
  I2C_STATE_ERROR,
} I2C_State;

typedef void (*i2c_callback_t)(I2C_TypeDef *i2c, HAL_Status status);

uint16_t hal_i2c_calc_ccr(uint32_t pclk_hz, I2C_Speed speed);

uint8_t hal_i2c_calc_trise(uint32_t pclk_hz, I2C_Speed speed);

HAL_Status hal_i2c_init(I2C_TypeDef *i2c, const I2C_Config *cfg);

void hal_i2c_deinit(I2C_TypeDef *i2c);

HAL_Status hal_i2c_probe(I2C_TypeDef *i2c, uint8_t addr);

HAL_Status hal_i2c_write_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                             uint8_t val);

HAL_Status hal_i2c_read_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                            uint8_t *buf, size_t len);

HAL_Status hal_i2c_mem_write(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                             uint8_t mem_sz, const uint8_t *buf, size_t len);

HAL_Status hal_i2c_mem_read(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                            uint8_t mem_sz, uint8_t *buf, size_t len);

HAL_Status hal_i2c_write(I2C_TypeDef *i2c, uint8_t addr, const uint8_t *buf,
                         size_t len);

HAL_Status hal_i2c_read(I2C_TypeDef *i2c, uint8_t addr, uint8_t *buf,
                        size_t len);

HAL_Status hal_i2c_start(I2C_TypeDef *i2c);

HAL_Status hal_i2c_repeated_start(I2C_TypeDef *i2c);

HAL_Status hal_i2c_send_addr(I2C_TypeDef *i2c, uint8_t addr, uint8_t rw);

HAL_Status hal_i2c_write_byte(I2C_TypeDef *i2c, uint8_t b);

HAL_Status hal_i2c_read_byte(I2C_TypeDef *i2c, uint8_t *b, bool ack);

void hal_i2c_stop(I2C_TypeDef *i2c);

HAL_Status hal_i2c_bus_recover(I2C_TypeDef *i2c, GPIO_TypeDef *port,
                               uint8_t scl, uint8_t sda, const I2C_Config *cfg);

void hal_i2c_register_callback(I2C_TypeDef *i2c, i2c_callback_t cb);
/* NVIC priority of both the event and error interrupts; 0 (the highest) until
 * set */
void hal_i2c_set_priority(I2C_TypeDef *i2c, uint8_t priority);

I2C_State hal_i2c_state(const I2C_TypeDef *i2c);

bool hal_i2c_busy(const I2C_TypeDef *i2c);

HAL_Status hal_i2c_write_it(I2C_TypeDef *i2c, uint8_t addr, const uint8_t *buf,
                            size_t len);

HAL_Status hal_i2c_read_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t *buf,
                           size_t len);

HAL_Status hal_i2c_write_reg_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                                uint8_t val);

HAL_Status hal_i2c_read_reg_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                               uint8_t *buf, size_t len);

HAL_Status hal_i2c_mem_write_it(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                                uint8_t mem_sz, const uint8_t *buf,
                                size_t len);

HAL_Status hal_i2c_mem_read_it(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                               uint8_t mem_sz, uint8_t *buf, size_t len);

/* give up on a running *_it transfer (e.g. after your own timeout): STOP, then
 * the callback with HAL_ERR_TIMEOUT. A slave still holding SCL keeps the bus
 * busy: hal_i2c_bus_recover() then. */
void hal_i2c_abort_it(I2C_TypeDef *i2c);

/* ---- interrupt state machine internals, exposed for host tests ---- */

typedef enum {
  I2C_STEP_IDLE,
  I2C_STEP_START,   /* waiting for SB */
  I2C_STEP_ADDR,    /* address sent, waiting for ADDR */
  I2C_STEP_TX,      /* pointer bytes, then tx */
  I2C_STEP_RESTART, /* repeated START requested: only SB counts */
  I2C_STEP_RX,
} I2C_Step;

/* one transfer: write ptr then tx, repeated START, read rx (either half may
 * be empty). tx and rx must stay valid until the callback. */
typedef struct {
  volatile I2C_State state;
  I2C_Step step;
  bool reading;
  uint8_t addr; /* 7-bit */
  uint8_t ptr[2];
  uint8_t ptr_len;
  const uint8_t *tx;
  size_t tx_len;
  uint8_t *rx;
  size_t rx_len;
  size_t n; /* bytes done in the current direction */
  HAL_Status result;
  i2c_callback_t cb;
} I2C_Xfer;

void hal_i2c_xfer_prepare(I2C_Xfer *x, uint8_t addr, const uint8_t *ptr,
                          uint8_t ptr_len, const uint8_t *tx, size_t tx_len,
                          uint8_t *rx, size_t rx_len);
void hal_i2c_ev_step(I2C_TypeDef *i2c, I2C_Xfer *x);
void hal_i2c_er_step(I2C_TypeDef *i2c, I2C_Xfer *x);

#endif /* HAL_I2C_H */
