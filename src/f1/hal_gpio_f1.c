/**
 * @file hal_gpio_f1.c
 * @brief STM32F1 GPIO implementation
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_error.h"
#include "hal_gpio.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

static uint8_t gpio_mode_cnf(GPIO_Mode mode) {
  switch (mode) {
  case GPIO_MODE_ANALOG:
    return 0b00;
  case GPIO_MODE_INPUT:
    return 0b01;
  case GPIO_MODE_INPUT_PULL:
    return 0b10;
  case GPIO_MODE_OUTPUT_PP:
    return 0b00;
  case GPIO_MODE_OUTPUT_OD:
    return 0b01;
  case GPIO_MODE_AF_PP:
    return 0b10;
  case GPIO_MODE_AF_OD:
    return 0b11;
  }
  return 0b00;
}

static bool gpio_mode_is_output(GPIO_Mode mode) {
  return mode == GPIO_MODE_OUTPUT_PP || mode == GPIO_MODE_OUTPUT_OD ||
         mode == GPIO_MODE_AF_PP || mode == GPIO_MODE_AF_OD;
}

uint8_t gpio_encode_config(GPIO_Mode mode, GPIO_Speed speed, GPIO_Pull pull) {
  (void)pull;
  uint8_t cnf = gpio_mode_cnf(mode);
  uint8_t mode_bits = gpio_mode_is_output(mode) ? (uint8_t)speed : 0u;
  return (uint8_t)((cnf << 2) | mode_bits);
}

void hal_gpio_pin_init(GPIO_TypeDef *port, uint8_t pin,
                       const GPIO_Config *cfg) {
  HAL_ASSERT(pin < 16U);
  volatile uint32_t *cr = gpio_cr_reg(port, pin);
  uint8_t shift = gpio_cr_shift(pin);
  uint8_t nibble = gpio_encode_config(cfg->mode, cfg->speed, cfg->pull);

  *cr = (*cr & ~(0xFU << shift)) | ((uint32_t)nibble << shift);

  if (cfg->mode == GPIO_MODE_INPUT_PULL) {
    if (cfg->pull == GPIO_PULL_UP)
      port->BSRR = (1U << pin);
    else
      port->BRR = (1U << pin);
  }
}

void hal_gpio_pin_deinit(GPIO_TypeDef *port, uint8_t pin) {
  HAL_ASSERT(pin < 16U);

  volatile uint32_t *cr = gpio_cr_reg(port, pin);
  uint8_t shift = gpio_cr_shift(pin);

  *cr = (*cr & ~(0xFU << shift)) | ((uint32_t)0b0100U << shift);

  port->BRR = (1U << pin);
}

void hal_gpio_pin_lock(GPIO_TypeDef *port, uint8_t pin) {
  HAL_ASSERT(pin < 16U);

  uint32_t lckk = (1U << 16);
  uint32_t lck = lckk | (1U << pin);

  port->LCKR = lck;
  port->LCKR = (1U << pin);
  port->LCKR = lck;
  (void)port->LCKR;
  (void)port->LCKR;
}

/* New MAPR value with one remap field changed. SWJ_CFG is write-only and reads
 * as undefined, so writing it back could disable SWD (RM0008 AFIO_MAPR). It is
 * always cleared here instead, which writes 000: the reset state, SWD on. */
uint32_t hal_afio_mapr_next(uint32_t mapr, uint32_t field_mask,
                            uint32_t field_value) {
  return (mapr & ~(field_mask | AFIO_MAPR_SWJ_CFG)) | field_value;
}

void hal_afio_enable(void) { RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; }

void hal_afio_remap(uint32_t field_mask, uint32_t field_value) {
  HAL_ASSERT((field_mask & AFIO_MAPR_SWJ_CFG) == 0U);
  HAL_ASSERT((field_value & ~field_mask) == 0U);

  hal_afio_enable();
  AFIO->MAPR = hal_afio_mapr_next(AFIO->MAPR, field_mask, field_value);
}
