/**
 * @file hal_i2c_f1.c
 * @brief STM32F1 I2C master implementation (blocking, Sm + Fm)
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * Every blocking wait is bounded by the per-instance timeout in ms, so an
 * absent or stuck device returns an error instead of hanging. The timeout comes
 * from the SysTick counter, so call hal_systick_init() before hal_i2c_init(),
 * or these waits will never expire
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define I2C_COUNT 2
#define I2C_WRITE 0U
#define I2C_READ 1U

static uint32_t i2c_timeouts[I2C_COUNT];

static I2C_Xfer i2c_xfers[I2C_COUNT]; /* *_it transfer, one per instance */

static int i2c_index(const I2C_TypeDef *i2c) {
  if (i2c == I2C1) {
    return 0;
  }
  if (i2c == I2C2) {
    return 1;
  }
  return -1;
}

static const IRQn_Type i2c_ev_irq[I2C_COUNT] = {I2C1_EV_IRQn, I2C2_EV_IRQn};
static const IRQn_Type i2c_er_irq[I2C_COUNT] = {I2C1_ER_IRQn, I2C2_ER_IRQn};

static uint32_t i2c_timeout(const I2C_TypeDef *i2c) {
  int idx = i2c_index(i2c);
  return (idx >= 0) ? i2c_timeouts[idx] : 0U;
}

/* wait until (SR1 & flag) is set, bounded by the instance timeout. Also aborts
 * early on an acknowledge failure (AF), which the caller maps to HAL_ERR_NACK.
 */
static HAL_Status i2c_wait_sr1(I2C_TypeDef *i2c, uint32_t flag) {
  uint32_t start = hal_get_tick();
  uint32_t timeout_ms = i2c_timeout(i2c);

  while (!(i2c->SR1 & flag)) {
    if (i2c->SR1 & I2C_SR1_AF) {
      i2c->SR1 = ~(uint32_t)I2C_SR1_AF; /* rc_w0, see timer_dispatch */
      i2c->CR1 |= I2C_CR1_STOP;
      return HAL_ERR_NACK;
    }
    if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return HAL_OK;
}

HAL_Status hal_i2c_init(I2C_TypeDef *i2c, const I2C_Config *cfg) {
  int idx = i2c_index(i2c);
  HAL_ASSERT(idx >= 0);
  if (idx < 0) {
    return HAL_ERR_UNSUPPORTED;
  }

  i2c_timeouts[idx] = cfg->timeout_ms;
  i2c_xfers[idx].state = I2C_STATE_READY;
  i2c_xfers[idx].step = I2C_STEP_IDLE;

  if (i2c == I2C1) {
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  } else {
    RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
  }

  /* software reset clears any stuck BUSY/state (also leaves NOSTRETCH clear, so
   * clock stretching stays enabled: a slave holding SCL low just delays a flag,
   * which the timeout-bounded waits tolerate -- size cfg->timeout_ms for the
   * slowest-stretching device on the bus). */
  i2c->CR1 |= I2C_CR1_SWRST;
  i2c->CR1 &= ~I2C_CR1_SWRST;

  uint32_t pclk = hal_clock_get_pclk1_hz();

  i2c->CR2 = (uint32_t)(pclk / 1000000U); /* FREQ in MHz */

  uint16_t ccr = hal_i2c_calc_ccr(pclk, cfg->speed);
  if (cfg->speed == I2C_SPEED_FAST) {
    i2c->CCR = I2C_CCR_FS | ccr;
  } else {
    i2c->CCR = ccr;
  }

  i2c->TRISE = hal_i2c_calc_trise(pclk, cfg->speed);

  i2c->CR1 |= I2C_CR1_PE;

  return HAL_OK;
}

void hal_i2c_deinit(I2C_TypeDef *i2c) {
  i2c->CR1 &= ~I2C_CR1_PE;
  if (i2c == I2C1) {
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
  } else if (i2c == I2C2) {
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C2EN;
  }
}

/* primitives */

/* wait for the bus to be free (SR2.BUSY clear) before starting a transaction,
 * so a still-settling STOP from a previous transfer (or another master) can't
 * corrupt this one. Bounded by the instance timeout. */
static HAL_Status i2c_wait_not_busy(I2C_TypeDef *i2c) {
  uint32_t start = hal_get_tick();
  uint32_t timeout_ms = i2c_timeout(i2c);

  while (i2c->SR2 & I2C_SR2_BUSY) {
    if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
      return HAL_ERR_BUS;
    }
  }
  return HAL_OK;
}

HAL_Status hal_i2c_start(I2C_TypeDef *i2c) {
  HAL_Status st = i2c_wait_not_busy(i2c);
  if (st != HAL_OK) {
    return st;
  }

  i2c->CR1 |= I2C_CR1_ACK;
  i2c->CR1 |= I2C_CR1_START;
  return i2c_wait_sr1(i2c, I2C_SR1_SB);
}

HAL_Status hal_i2c_repeated_start(I2C_TypeDef *i2c) {
  /* the bus is legitimately busy mid-transaction here so
   * this must NOT wait for BUSY to clear, just issue another START. */
  i2c->CR1 |= I2C_CR1_ACK;
  i2c->CR1 |= I2C_CR1_START;
  return i2c_wait_sr1(i2c, I2C_SR1_SB);
}

HAL_Status hal_i2c_send_addr(I2C_TypeDef *i2c, uint8_t addr, uint8_t rw) {
  i2c->DR = (uint32_t)(((uint32_t)addr << 1) | (rw & 1U));

  HAL_Status st = i2c_wait_sr1(i2c, I2C_SR1_ADDR);
  if (st != HAL_OK) {
    return st;
  }

  (void)i2c->SR1; /* clear ADDR by reading SR1 then SR2 */
  (void)i2c->SR2;
  return HAL_OK;
}

HAL_Status hal_i2c_write_byte(I2C_TypeDef *i2c, uint8_t b) {
  HAL_Status st = i2c_wait_sr1(i2c, I2C_SR1_TXE);
  if (st != HAL_OK) {
    return st;
  }
  i2c->DR = b;
  return HAL_OK;
}

HAL_Status hal_i2c_read_byte(I2C_TypeDef *i2c, uint8_t *b, bool ack) {
  if (ack) {
    i2c->CR1 |= I2C_CR1_ACK;
  } else {
    i2c->CR1 &= ~I2C_CR1_ACK;
  }

  HAL_Status st = i2c_wait_sr1(i2c, I2C_SR1_RXNE);
  if (st != HAL_OK) {
    return st;
  }
  *b = (uint8_t)i2c->DR;
  return HAL_OK;
}

void hal_i2c_stop(I2C_TypeDef *i2c) { i2c->CR1 |= I2C_CR1_STOP; }

/* transactions */

HAL_Status hal_i2c_write(I2C_TypeDef *i2c, uint8_t addr, const uint8_t *buf,
                         uint32_t len) {
  HAL_ASSERT(i2c_index(i2c) >= 0);

  HAL_Status st = hal_i2c_start(i2c);
  if (st != HAL_OK) {
    return st;
  }

  st = hal_i2c_send_addr(i2c, addr, I2C_WRITE);
  if (st != HAL_OK) {
    return st;
  }

  for (uint32_t i = 0U; i < len; i++) {
    st = hal_i2c_write_byte(i2c, buf[i]);
    if (st != HAL_OK) {
      return st;
    }
  }

  /* wait for the last byte to shift out before STOP */
  st = i2c_wait_sr1(i2c, I2C_SR1_BTF);
  hal_i2c_stop(i2c);
  return st;
}

/* read `len` bytes after a START/repeated-START has already been issued. Sends
 * the read address and handles the N=1 / N=2 / N>=3 special sequencing
 * for ACK/STOP around the ADDR clear, the F1 peripheral requires the last
 * ACK/STOP to be armed before the final byte(s) are clocked in. */
static HAL_Status i2c_read_into(I2C_TypeDef *i2c, uint8_t addr, uint8_t *buf,
                                uint32_t len) {
  i2c->DR = (uint32_t)(((uint32_t)addr << 1) | I2C_READ);
  HAL_Status st = i2c_wait_sr1(i2c, I2C_SR1_ADDR);
  if (st != HAL_OK) {
    return st;
  }

  if (len == 1U) {
    i2c->CR1 &= ~I2C_CR1_ACK; /* NACK the single byte */
    (void)i2c->SR1;           /* clear ADDR (SR1 then SR2) */
    (void)i2c->SR2;
    i2c->CR1 |= I2C_CR1_STOP; /* STOP armed before the byte arrives */

    st = i2c_wait_sr1(i2c, I2C_SR1_RXNE);
    if (st != HAL_OK) {
      return st;
    }
    buf[0] = (uint8_t)i2c->DR;
    return HAL_OK;
  }

  if (len == 2U) {
    i2c->CR1 |= I2C_CR1_POS; /* NACK the 2nd (last) byte */
    i2c->CR1 &= ~I2C_CR1_ACK;
    (void)i2c->SR1; /* clear ADDR */
    (void)i2c->SR2;

    st = i2c_wait_sr1(i2c, I2C_SR1_BTF); /* both bytes latched */
    if (st != HAL_OK) {
      return st;
    }
    i2c->CR1 |= I2C_CR1_STOP;
    buf[0] = (uint8_t)i2c->DR;
    buf[1] = (uint8_t)i2c->DR;
    i2c->CR1 &= ~I2C_CR1_POS;
    return HAL_OK;
  }

  /* len >= 3: ACK the run, then the tail (N-2 handling). */
  (void)i2c->SR1; /* clear ADDR */
  (void)i2c->SR2;

  for (uint32_t i = 0U; i < len; i++) {
    if (i == (len - 3U)) {
      /* wait for BTF, then NACK + STOP */
      st = i2c_wait_sr1(i2c, I2C_SR1_BTF);
      if (st != HAL_OK) {
        return st;
      }
      i2c->CR1 &= ~I2C_CR1_ACK;
      buf[i] = (uint8_t)i2c->DR;
      i++;
      st = i2c_wait_sr1(i2c, I2C_SR1_BTF);
      if (st != HAL_OK) {
        return st;
      }
      i2c->CR1 |= I2C_CR1_STOP;
      buf[i] = (uint8_t)i2c->DR;
      i++;
      st = i2c_wait_sr1(i2c, I2C_SR1_RXNE);
      if (st != HAL_OK) {
        return st;
      }
      buf[i] = (uint8_t)i2c->DR;
      break;
    }

    st = i2c_wait_sr1(i2c, I2C_SR1_RXNE);
    if (st != HAL_OK) {
      return st;
    }
    buf[i] = (uint8_t)i2c->DR;
  }
  return HAL_OK;
}

HAL_Status hal_i2c_read(I2C_TypeDef *i2c, uint8_t addr, uint8_t *buf,
                        uint32_t len) {
  HAL_ASSERT(i2c_index(i2c) >= 0);
  if (len == 0U) {
    return HAL_OK;
  }

  i2c->CR1 |= I2C_CR1_ACK;
  HAL_Status st = hal_i2c_start(i2c);
  if (st != HAL_OK) {
    return st;
  }
  return i2c_read_into(i2c, addr, buf, len);
}

/* memory helpers */

HAL_Status hal_i2c_probe(I2C_TypeDef *i2c, uint8_t addr) {
  HAL_Status st = hal_i2c_start(i2c);
  if (st != HAL_OK) {
    return st;
  }
  st = hal_i2c_send_addr(i2c, addr, I2C_WRITE);
  hal_i2c_stop(i2c);
  return st;
}

HAL_Status hal_i2c_write_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                             uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return hal_i2c_write(i2c, addr, buf, 2U);
}

HAL_Status hal_i2c_read_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint32_t len) {
  return hal_i2c_mem_read(i2c, addr, reg, 1U, buf, len);
}

HAL_Status hal_i2c_mem_write(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                             uint8_t mem_sz, const uint8_t *buf, uint32_t len) {
  HAL_ASSERT(mem_sz == 1U || mem_sz == 2U);

  HAL_Status st = hal_i2c_start(i2c);
  if (st != HAL_OK) {
    return st;
  }
  st = hal_i2c_send_addr(i2c, addr, I2C_WRITE);
  if (st != HAL_OK) {
    return st;
  }

  if (mem_sz == 2U) {
    st = hal_i2c_write_byte(i2c, (uint8_t)(mem >> 8));
    if (st != HAL_OK) {
      return st;
    }
  }
  st = hal_i2c_write_byte(i2c, (uint8_t)(mem & 0xFFU));
  if (st != HAL_OK) {
    return st;
  }

  for (uint32_t i = 0U; i < len; i++) {
    st = hal_i2c_write_byte(i2c, buf[i]);
    if (st != HAL_OK) {
      return st;
    }
  }

  st = i2c_wait_sr1(i2c, I2C_SR1_BTF);
  hal_i2c_stop(i2c);
  return st;
}

HAL_Status hal_i2c_mem_read(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                            uint8_t mem_sz, uint8_t *buf, uint32_t len) {
  HAL_ASSERT(mem_sz == 1U || mem_sz == 2U);
  if (len == 0U) {
    return HAL_OK;
  }

  /* write the register pointer */
  HAL_Status st = hal_i2c_start(i2c);
  if (st != HAL_OK) {
    return st;
  }
  st = hal_i2c_send_addr(i2c, addr, I2C_WRITE);
  if (st != HAL_OK) {
    return st;
  }
  if (mem_sz == 2U) {
    st = hal_i2c_write_byte(i2c, (uint8_t)(mem >> 8));
    if (st != HAL_OK) {
      return st;
    }
  }
  st = hal_i2c_write_byte(i2c, (uint8_t)(mem & 0xFFU));
  if (st != HAL_OK) {
    return st;
  }
  st = i2c_wait_sr1(i2c, I2C_SR1_BTF);
  if (st != HAL_OK) {
    return st;
  }

  /* repeated START, then read from that pointer */
  i2c->CR1 |= I2C_CR1_ACK;
  st = hal_i2c_repeated_start(i2c);
  if (st != HAL_OK) {
    return st;
  }
  return i2c_read_into(i2c, addr, buf, len);
}

/* bus recovery */

/* free a slave that is holding SDA low:
 * drive SCL manually as GPIO for up to 9 clocks until SDA is released, emit a
 * manual STOP, then re-init the peripheral. */
HAL_Status hal_i2c_bus_recover(I2C_TypeDef *i2c, GPIO_TypeDef *port,
                               uint8_t scl, uint8_t sda,
                               const I2C_Config *cfg) {
  HAL_ASSERT(i2c_index(i2c) >= 0);

  i2c->CR1 &= ~I2C_CR1_PE; /* release the peripheral's hold on the pins */

  /* released (high) before they become outputs, or SCL glitches low */
  hal_gpio_pin_write(port, scl, 1U);
  hal_gpio_pin_write(port, sda, 1U);
  GPIO_Config od = {GPIO_MODE_OUTPUT_OD, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  hal_gpio_pin_init(port, scl, &od);
  hal_gpio_pin_init(port, sda, &od);

  /* up to 9 clocks to flush a stuck slave that is mid-byte */
  for (uint32_t i = 0U; i < 9U; i++) {
    if (hal_gpio_pin_read(port, sda)) {
      break; /* slave let go */
    }
    hal_gpio_pin_write(port, scl, 0U);
    for (volatile uint32_t d = 0U; d < 100U; d++) {
    }
    hal_gpio_pin_write(port, scl, 1U);
    for (volatile uint32_t d = 0U; d < 100U; d++) {
    }
  }

  /* manual STOP: SDA low -> high while SCL high */
  hal_gpio_pin_write(port, sda, 0U);
  for (volatile uint32_t d = 0U; d < 100U; d++) {
  }
  hal_gpio_pin_write(port, scl, 1U);
  for (volatile uint32_t d = 0U; d < 100U; d++) {
  }
  hal_gpio_pin_write(port, sda, 1U);

  /* hand the pins back to the peripheral (AF open-drain) and re-init */
  GPIO_Config af = {GPIO_MODE_AF_OD, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};
  hal_gpio_pin_init(port, scl, &af);
  hal_gpio_pin_init(port, sda, &af);

  return hal_i2c_init(i2c, cfg);
}

/* interrupt-driven transfers
 *
 * Reception ends with the RM0008 BTF sequences (N=1, N=2 with POS, N>=3 from
 * N-2 on): SCL is held while BTF is pending, so a late ISR stretches the bus
 * instead of losing or adding bytes. ITBUFEN is on only while a TXE/RXNE per
 * byte is wanted. */

#define I2C_IT_ALL (I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN)

static I2C_Xfer *i2c_xfer(I2C_TypeDef *i2c) {
  int idx = i2c_index(i2c);
  return (idx >= 0) ? &i2c_xfers[idx] : NULL;
}

void hal_i2c_register_callback(I2C_TypeDef *i2c, i2c_callback_t cb) {
  I2C_Xfer *x = i2c_xfer(i2c);
  if (x != NULL) {
    x->cb = cb;
  }
}

I2C_State hal_i2c_state(I2C_TypeDef *i2c) {
  I2C_Xfer *x = i2c_xfer(i2c);
  return (x != NULL) ? x->state : I2C_STATE_ERROR;
}

bool hal_i2c_is_busy(I2C_TypeDef *i2c) {
  return hal_i2c_state(i2c) == I2C_STATE_BUSY;
}

void hal_i2c_xfer_prepare(I2C_Xfer *x, uint8_t addr, const uint8_t *ptr,
                          uint8_t ptr_len, const uint8_t *tx, uint32_t tx_len,
                          uint8_t *rx, uint32_t rx_len) {
  HAL_ASSERT(ptr_len <= 2U);
  HAL_ASSERT((ptr != NULL) || (ptr_len == 0U));
  HAL_ASSERT((tx != NULL) || (tx_len == 0U));
  HAL_ASSERT((rx != NULL) || (rx_len == 0U));

  x->addr = addr;
  for (uint8_t i = 0U; i < ptr_len; i++) {
    x->ptr[i] = ptr[i]; /* copied: the caller's may be a local */
  }
  x->ptr_len = ptr_len;
  x->tx = tx;
  x->tx_len = tx_len;
  x->rx = rx;
  x->rx_len = rx_len;
  x->reading = ((((uint32_t)ptr_len + tx_len) == 0U) && (rx_len > 0U));
  x->n = 0U;
  x->step = I2C_STEP_START;
  x->result = HAL_OK;
  x->state = I2C_STATE_BUSY;
}

static void it_finish(I2C_TypeDef *i2c, I2C_Xfer *x, HAL_Status result) {
  i2c->CR2 &= ~I2C_IT_ALL;
  i2c->CR1 &= ~I2C_CR1_POS;
  x->step = I2C_STEP_IDLE;
  x->result = result;
  x->state = (result == HAL_OK) ? I2C_STATE_READY : I2C_STATE_ERROR;
  if (x->cb != NULL) {
    x->cb(i2c, result);
  }
}

static void clear_addr(I2C_TypeDef *i2c) {
  (void)i2c->SR1; /* SR1 then SR2 */
  (void)i2c->SR2;
}

static void on_addr(I2C_TypeDef *i2c, I2C_Xfer *x) {
  if (!x->reading) {
    clear_addr(i2c);
    if (((uint32_t)x->ptr_len + x->tx_len) == 0U) { /* address only: a probe */
      i2c->CR1 |= I2C_CR1_STOP;
      it_finish(i2c, x, HAL_OK);
    } else {
      x->step = I2C_STEP_TX;
      i2c->CR2 |= I2C_CR2_ITBUFEN; /* one TXE per byte */
    }
    return;
  }

  x->step = I2C_STEP_RX;
  x->n = 0U;
  if (x->rx_len == 1U) {
    i2c->CR1 &= ~I2C_CR1_ACK;
    clear_addr(i2c);
    i2c->CR1 |= I2C_CR1_STOP; /* must land before the byte ends */
    i2c->CR2 |= I2C_CR2_ITBUFEN;
  } else if (x->rx_len == 2U) {
    i2c->CR1 = (i2c->CR1 & ~I2C_CR1_ACK) | I2C_CR1_POS; /* NACK the 2nd */
    clear_addr(i2c);
    i2c->CR2 &= ~I2C_CR2_ITBUFEN; /* wait for BTF: both bytes in */
  } else {
    i2c->CR1 |= I2C_CR1_ACK;
    clear_addr(i2c);
    if (x->rx_len > 3U) {
      i2c->CR2 |= I2C_CR2_ITBUFEN;
    } else {
      i2c->CR2 &= ~I2C_CR2_ITBUFEN;
    }
  }
}

static void on_tx(I2C_TypeDef *i2c, I2C_Xfer *x, uint32_t sr1) {
  uint32_t total = (uint32_t)x->ptr_len + x->tx_len;

  if (x->n < total) {
    if ((sr1 & I2C_SR1_TXE) != 0U) {
      i2c->DR = (x->n < x->ptr_len) ? x->ptr[x->n] : x->tx[x->n - x->ptr_len];
      x->n++;
      if (x->n == total) {
        i2c->CR2 &= ~I2C_CR2_ITBUFEN; /* next event: BTF, last byte out */
      }
    }
    return;
  }
  if ((sr1 & I2C_SR1_BTF) == 0U) {
    return;
  }
  if (x->rx_len > 0U) {
    x->reading = true;
    x->step = I2C_STEP_RESTART;
    i2c->CR1 |= I2C_CR1_START;
  } else {
    i2c->CR1 |= I2C_CR1_STOP;
    it_finish(i2c, x, HAL_OK);
  }
}

static void on_rx(I2C_TypeDef *i2c, I2C_Xfer *x, uint32_t sr1) {
  if (x->rx_len == 2U) {
    if ((sr1 & I2C_SR1_BTF) != 0U) {
      i2c->CR1 |= I2C_CR1_STOP;
      x->rx[0] = (uint8_t)i2c->DR;
      x->rx[1] = (uint8_t)i2c->DR;
      x->n = 2U;
      it_finish(i2c, x, HAL_OK);
    }
    return;
  }

  if ((x->rx_len - x->n) == 3U) {
    if ((sr1 & I2C_SR1_BTF) != 0U) { /* N-2 in DR, N-1 in shift, SCL held */
      i2c->CR1 &= ~I2C_CR1_ACK;
      x->rx[x->n++] = (uint8_t)i2c->DR;
      i2c->CR1 |= I2C_CR1_STOP;
      x->rx[x->n++] = (uint8_t)i2c->DR;
      i2c->CR2 |= I2C_CR2_ITBUFEN; /* last byte by RXNE */
    }
    return;
  }

  if ((sr1 & I2C_SR1_RXNE) != 0U) {
    HAL_ASSERT(x->n < x->rx_len);
    x->rx[x->n++] = (uint8_t)i2c->DR;
    if (x->n == x->rx_len) {
      it_finish(i2c, x, HAL_OK);
    } else if ((x->rx_len - x->n) == 3U) {
      i2c->CR2 &= ~I2C_CR2_ITBUFEN; /* the tail goes by BTF */
    } else {
      /* more to come */
    }
  }
}

void hal_i2c_ev_step(I2C_TypeDef *i2c, I2C_Xfer *x) {
  uint32_t sr1 = i2c->SR1;

  switch (x->step) {
  case I2C_STEP_START:
  case I2C_STEP_RESTART:
    /* TXE/BTF left over from the pointer write stay set until the repeated
     * START goes out: only SB means anything here */
    if ((sr1 & I2C_SR1_SB) != 0U) {
      i2c->DR = ((uint32_t)x->addr << 1) | (x->reading ? I2C_READ : I2C_WRITE);
      x->step = I2C_STEP_ADDR;
    }
    break;
  case I2C_STEP_ADDR:
    if ((sr1 & I2C_SR1_ADDR) != 0U) {
      on_addr(i2c, x);
    }
    break;
  case I2C_STEP_TX:
    on_tx(i2c, x, sr1);
    break;
  case I2C_STEP_RX:
    on_rx(i2c, x, sr1);
    break;
  default:
    i2c->CR2 &= ~I2C_IT_ALL; /* nothing running */
    break;
  }
}

void hal_i2c_er_step(I2C_TypeDef *i2c, I2C_Xfer *x) {
  uint32_t sr1 = i2c->SR1;
  uint32_t errs =
      sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR);

  i2c->SR1 = ~errs; /* rc_w0: clear exactly these */
  if (x->step == I2C_STEP_IDLE) {
    i2c->CR2 &= ~I2C_IT_ALL;
    return;
  }
  if ((sr1 & I2C_SR1_ARLO) == 0U) { /* lost arbitration: not our bus to STOP */
    i2c->CR1 |= I2C_CR1_STOP;
  }
  it_finish(i2c, x, ((sr1 & I2C_SR1_AF) != 0U) ? HAL_ERR_NACK : HAL_ERR_BUS);
}

/* busy check first: a call while busy must not touch the running transfer */
static HAL_Status it_begin(I2C_TypeDef *i2c, uint8_t addr, const uint8_t *ptr,
                           uint8_t ptr_len, const uint8_t *tx, uint32_t tx_len,
                           uint8_t *rx, uint32_t rx_len) {
  int idx = i2c_index(i2c);
  HAL_ASSERT(idx >= 0);
  if (idx < 0) {
    return HAL_ERR_UNSUPPORTED;
  }
  I2C_Xfer *x = &i2c_xfers[idx];
  if (x->state == I2C_STATE_BUSY) {
    return HAL_ERR_BUSY;
  }
  HAL_Status st = i2c_wait_not_busy(i2c);
  if (st != HAL_OK) {
    return st;
  }

  hal_i2c_xfer_prepare(x, addr, ptr, ptr_len, tx, tx_len, rx, rx_len);
  i2c->CR1 = (i2c->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;
  i2c->CR2 = (i2c->CR2 & ~I2C_CR2_ITBUFEN) | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
  __NVIC_EnableIRQ(i2c_ev_irq[idx]);
  __NVIC_EnableIRQ(i2c_er_irq[idx]);
  i2c->CR1 |= I2C_CR1_START; /* SB -> ISR */
  return HAL_OK;
}

HAL_Status hal_i2c_write_it(I2C_TypeDef *i2c, uint8_t addr, const uint8_t *buf,
                            uint32_t len) {
  return it_begin(i2c, addr, NULL, 0U, buf, len, NULL, 0U);
}

HAL_Status hal_i2c_read_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t *buf,
                           uint32_t len) {
  HAL_ASSERT(len > 0U); /* a zero-length read would never call back */
  if (len == 0U) {
    return HAL_ERR_PARAM;
  }
  return it_begin(i2c, addr, NULL, 0U, NULL, 0U, buf, len);
}

HAL_Status hal_i2c_write_reg_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                                uint8_t val) {
  const uint8_t p[2] = {reg, val};
  return it_begin(i2c, addr, p, 2U, NULL, 0U, NULL, 0U);
}

HAL_Status hal_i2c_read_reg_it(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg,
                               uint8_t *buf, uint32_t len) {
  return hal_i2c_mem_read_it(i2c, addr, reg, 1U, buf, len);
}

HAL_Status hal_i2c_mem_write_it(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                                uint8_t mem_sz, const uint8_t *buf,
                                uint32_t len) {
  HAL_ASSERT((mem_sz == 1U) || (mem_sz == 2U));
  const uint8_t p[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFFU)};
  return it_begin(i2c, addr, &p[2U - mem_sz], mem_sz, buf, len, NULL, 0U);
}

HAL_Status hal_i2c_mem_read_it(I2C_TypeDef *i2c, uint8_t addr, uint16_t mem,
                               uint8_t mem_sz, uint8_t *buf, uint32_t len) {
  HAL_ASSERT((mem_sz == 1U) || (mem_sz == 2U));
  HAL_ASSERT(len > 0U);
  if (len == 0U) {
    return HAL_ERR_PARAM;
  }
  const uint8_t p[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFFU)};
  return it_begin(i2c, addr, &p[2U - mem_sz], mem_sz, NULL, 0U, buf, len);
}

void hal_i2c_abort_it(I2C_TypeDef *i2c) {
  I2C_Xfer *x = i2c_xfer(i2c);
  if (x == NULL) {
    return;
  }
  x->step = I2C_STEP_IDLE; /* a late ISR now does nothing */
  i2c->CR2 &= ~I2C_IT_ALL;
  if (x->state != I2C_STATE_BUSY) {
    return; /* finished before we got here */
  }
  i2c->CR1 |= I2C_CR1_STOP;
  it_finish(i2c, x, HAL_ERR_TIMEOUT);
}

void I2C1_EV_IRQHandler(void) { hal_i2c_ev_step(I2C1, &i2c_xfers[0]); }
void I2C1_ER_IRQHandler(void) { hal_i2c_er_step(I2C1, &i2c_xfers[0]); }
void I2C2_EV_IRQHandler(void) { hal_i2c_ev_step(I2C2, &i2c_xfers[1]); }
void I2C2_ER_IRQHandler(void) { hal_i2c_er_step(I2C2, &i2c_xfers[1]); }
