/**
 * @file test_i2c.c
 * @brief Host-side unit tests for the F1 I2C timing math
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Timing math, plus the interrupt state machine driven on an I2C_TypeDef in
 * host RAM: each step sets the SR1 flags the hardware would raise and checks
 * what the driver wrote back. The blocking transactions are not host-callable.
 *
 * At pclk1 = 36 MHz:
 *   Sm (100 kHz): CCR = 36e6/(2*100e3) = 180, TRISE = 36 + 1 = 37
 *   Fm (400 kHz): CCR = 36e6/(3*400e3) = 30,  TRISE = 36*300/1000 + 1 = 11
 */

#include "hal_i2c.h"
#include "test_assert.h"
#include <string.h>

#define PCLK1 36000000U

void test_i2c_ccr_standard(void) {
  ASSERT_EQ(hal_i2c_calc_ccr(PCLK1, I2C_SPEED_STANDARD), 180U);
}

void test_i2c_ccr_fast(void) {
  ASSERT_EQ(hal_i2c_calc_ccr(PCLK1, I2C_SPEED_FAST), 30U);
}

void test_i2c_trise_standard(void) {
  ASSERT_EQ(hal_i2c_calc_trise(PCLK1, I2C_SPEED_STANDARD), 37U);
}

void test_i2c_trise_fast(void) {
  ASSERT_EQ(hal_i2c_calc_trise(PCLK1, I2C_SPEED_FAST), 11U);
}

/* ---- interrupt state machine ---- */

#define IT_ALL (I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN)

static unsigned cb_calls;
static HAL_Status cb_status;

static void on_done(I2C_TypeDef *i2c, HAL_Status st) {
  (void)i2c;
  cb_calls++;
  cb_status = st;
}

static void fresh(I2C_TypeDef *r, I2C_Xfer *x) {
  memset(r, 0, sizeof *r);
  memset(x, 0, sizeof *x);
  x->cb = on_done;
  cb_calls = 0U;
  cb_status = HAL_ERR_UNSUPPORTED;
}

static void ev(I2C_TypeDef *r, I2C_Xfer *x, uint32_t sr1) {
  r->SR1 = sr1;
  hal_i2c_ev_step(r, x);
}

static void check_done_ok(const I2C_TypeDef *r, const I2C_Xfer *x) {
  ASSERT_EQ(cb_calls, 1U);
  ASSERT_EQ(cb_status, HAL_OK);
  ASSERT_EQ(x->state, I2C_STATE_READY);
  ASSERT_EQ(r->CR2 & IT_ALL, 0U);
  ASSERT_EQ(r->CR1 & I2C_CR1_POS, 0U);
  ASSERT_TRUE((r->CR1 & I2C_CR1_STOP) != 0U);
}

/* the bench failure: TXE/BTF still set while the repeated START is pending
 * used to be taken as "send more data" */
void test_i2c_it_read_reg_two_bytes(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  uint8_t rx[3] = {0U, 0U, 0xEEU};
  const uint8_t reg = 0x0CU;
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, &reg, 1U, NULL, 0U, rx, 2U);

  ev(&r, &x, I2C_SR1_SB);
  ASSERT_EQ(r.DR, 0x6CU); /* 0x36 write */
  ev(&r, &x, I2C_SR1_ADDR);
  ASSERT_TRUE((r.CR2 & I2C_CR2_ITBUFEN) != 0U);
  ev(&r, &x, I2C_SR1_TXE);
  ASSERT_EQ(r.DR, 0x0CU);
  ASSERT_EQ(r.CR2 & I2C_CR2_ITBUFEN, 0U);
  ev(&r, &x, I2C_SR1_TXE); /* still shifting: nothing to do */
  ev(&r, &x, I2C_SR1_TXE | I2C_SR1_BTF);
  ASSERT_TRUE((r.CR1 & I2C_CR1_START) != 0U);
  ASSERT_EQ(x.step, I2C_STEP_RESTART);

  ev(&r, &x, I2C_SR1_TXE | I2C_SR1_BTF); /* START not out yet */
  ev(&r, &x, I2C_SR1_TXE | I2C_SR1_BTF);
  ASSERT_EQ(r.DR, 0x0CU); /* nothing written */
  ASSERT_EQ(x.step, I2C_STEP_RESTART);

  ev(&r, &x, I2C_SR1_SB);
  ASSERT_EQ(r.DR, 0x6DU); /* 0x36 read */
  ev(&r, &x, I2C_SR1_ADDR);
  ASSERT_TRUE((r.CR1 & I2C_CR1_POS) != 0U);
  ASSERT_EQ(r.CR1 & I2C_CR1_ACK, 0U);
  ASSERT_EQ(r.CR1 & I2C_CR1_STOP, 0U);
  ASSERT_EQ(r.CR2 & I2C_CR2_ITBUFEN, 0U); /* waits for BTF */
  r.DR = 0xABU;
  ev(&r, &x, I2C_SR1_RXNE | I2C_SR1_BTF);
  ASSERT_EQ(rx[0], 0xABU);
  ASSERT_EQ(rx[1], 0xABU);
  check_done_ok(&r, &x);

  ev(&r, &x, I2C_SR1_RXNE); /* late event after the end */
  ASSERT_EQ(rx[2], 0xEEU);
  ASSERT_EQ(cb_calls, 1U);
}

void test_i2c_it_read_one_byte(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  uint8_t rx[2] = {0U, 0xEEU};
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, rx, 1U);

  ev(&r, &x, I2C_SR1_SB);
  ASSERT_EQ(r.DR, 0x6DU); /* read straight away */
  r.CR1 = I2C_CR1_ACK;
  ev(&r, &x, I2C_SR1_ADDR);
  ASSERT_EQ(r.CR1 & I2C_CR1_ACK, 0U); /* NACK + STOP armed at ADDR */
  ASSERT_TRUE((r.CR1 & I2C_CR1_STOP) != 0U);
  ASSERT_TRUE((r.CR2 & I2C_CR2_ITBUFEN) != 0U);
  r.DR = 0x42U;
  ev(&r, &x, I2C_SR1_RXNE);
  ASSERT_EQ(rx[0], 0x42U);
  ASSERT_EQ(rx[1], 0xEEU);
  check_done_ok(&r, &x);
}

void test_i2c_it_read_three_bytes(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  uint8_t rx[4] = {0U, 0U, 0U, 0xEEU};
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, rx, 3U);

  ev(&r, &x, I2C_SR1_SB);
  ev(&r, &x, I2C_SR1_ADDR);
  ASSERT_TRUE((r.CR1 & I2C_CR1_ACK) != 0U);
  ASSERT_EQ(r.CR2 & I2C_CR2_ITBUFEN, 0U); /* tail from the start: BTF */
  r.DR = 1U;
  ev(&r, &x, I2C_SR1_RXNE); /* byte 1 in DR, waiting for byte 2 */
  ASSERT_EQ(x.n, 0U);
  ev(&r, &x, I2C_SR1_RXNE | I2C_SR1_BTF);
  ASSERT_EQ(r.CR1 & I2C_CR1_ACK, 0U);
  ASSERT_TRUE((r.CR1 & I2C_CR1_STOP) != 0U);
  ASSERT_EQ(x.n, 2U);
  ASSERT_TRUE((r.CR2 & I2C_CR2_ITBUFEN) != 0U);
  r.DR = 3U;
  ev(&r, &x, I2C_SR1_RXNE);
  ASSERT_EQ(rx[2], 3U);
  ASSERT_EQ(rx[3], 0xEEU);
  check_done_ok(&r, &x);
}

void test_i2c_it_read_five_bytes(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  uint8_t rx[6] = {0U, 0U, 0U, 0U, 0U, 0xEEU};
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, rx, 5U);

  ev(&r, &x, I2C_SR1_SB);
  ev(&r, &x, I2C_SR1_ADDR);
  ASSERT_TRUE((r.CR2 & I2C_CR2_ITBUFEN) != 0U);
  for (uint8_t b = 1U; b <= 2U; b++) {
    r.DR = b;
    ev(&r, &x, I2C_SR1_RXNE);
  }
  ASSERT_EQ(rx[1], 2U);
  ASSERT_EQ(r.CR2 & I2C_CR2_ITBUFEN, 0U); /* 3 left: switch to BTF */
  ASSERT_TRUE((r.CR1 & I2C_CR1_ACK) != 0U);
  r.DR = 3U;
  ev(&r, &x, I2C_SR1_RXNE | I2C_SR1_BTF);
  ASSERT_EQ(r.CR1 & I2C_CR1_ACK, 0U);
  ASSERT_EQ(x.n, 4U);
  r.DR = 5U;
  ev(&r, &x, I2C_SR1_RXNE);
  ASSERT_EQ(rx[4], 5U);
  ASSERT_EQ(rx[5], 0xEEU);
  check_done_ok(&r, &x);
}

void test_i2c_it_write_ptr_then_data(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  const uint8_t ptr[2] = {0x00U, 0x10U};
  const uint8_t data[2] = {0xAAU, 0xBBU};
  const uint8_t want[4] = {0x00U, 0x10U, 0xAAU, 0xBBU};
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x50U, ptr, 2U, data, 2U, NULL, 0U);

  ev(&r, &x, I2C_SR1_SB);
  ASSERT_EQ(r.DR, 0xA0U);
  ev(&r, &x, I2C_SR1_ADDR);
  for (uint32_t i = 0U; i < 4U; i++) {
    ev(&r, &x, I2C_SR1_TXE);
    ASSERT_EQ(r.DR, want[i]);
  }
  ASSERT_EQ(r.CR2 & I2C_CR2_ITBUFEN, 0U);
  ASSERT_EQ(cb_calls, 0U); /* not before the last byte is out */
  ev(&r, &x, I2C_SR1_TXE | I2C_SR1_BTF);
  ASSERT_EQ(r.CR1 & I2C_CR1_START, 0U);
  check_done_ok(&r, &x);
}

void test_i2c_it_probe(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, NULL, 0U);

  ASSERT_TRUE(!x.reading); /* nothing to read: an address write */
  ev(&r, &x, I2C_SR1_SB);
  ev(&r, &x, I2C_SR1_ADDR);
  check_done_ok(&r, &x);
}

void test_i2c_it_errors(void) {
  I2C_TypeDef r;
  I2C_Xfer x;
  uint8_t rx[2];
  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, rx, 2U);
  ev(&r, &x, I2C_SR1_SB);

  r.SR1 = I2C_SR1_AF; /* address NACKed */
  hal_i2c_er_step(&r, &x);
  ASSERT_EQ(r.SR1, (uint32_t)~I2C_SR1_AF); /* only AF cleared */
  ASSERT_TRUE((r.CR1 & I2C_CR1_STOP) != 0U);
  ASSERT_EQ(cb_status, HAL_ERR_NACK);
  ASSERT_EQ(x.state, I2C_STATE_ERROR);
  ASSERT_EQ(r.CR2 & IT_ALL, 0U);

  fresh(&r, &x);
  hal_i2c_xfer_prepare(&x, 0x36U, NULL, 0U, NULL, 0U, rx, 2U);
  r.SR1 = I2C_SR1_ARLO;
  hal_i2c_er_step(&r, &x);
  ASSERT_EQ(r.CR1 & I2C_CR1_STOP, 0U); /* lost the bus: no STOP */
  ASSERT_EQ(cb_status, HAL_ERR_BUS);

  fresh(&r, &x); /* nothing running */
  r.CR2 = IT_ALL;
  ev(&r, &x, I2C_SR1_RXNE);
  ASSERT_EQ(r.CR2 & IT_ALL, 0U);
  ASSERT_EQ(cb_calls, 0U);
}
