/**
 * @file hal_gpio.h
 * @brief Public GPIO API
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_types.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  GPIO_MODE_ANALOG = 0,
  GPIO_MODE_INPUT,
  GPIO_MODE_INPUT_PULL,
  GPIO_MODE_OUTPUT_PP,
  GPIO_MODE_OUTPUT_OD,
  GPIO_MODE_AF_PP,
  GPIO_MODE_AF_OD
} GPIO_Mode;

typedef enum {
  GPIO_SPEED_INPUT = 0x00,
  GPIO_SPEED_10_MHZ = 0x01,
  GPIO_SPEED_2_MHZ = 0x02,
  GPIO_SPEED_50_MHZ = 0x03

} GPIO_Speed;

typedef enum {
  GPIO_PULL_NONE = 0,
  GPIO_PULL_UP = 1,
  GPIO_PULL_DOWN = 2
} GPIO_Pull;

typedef struct {
  GPIO_Mode mode;
  GPIO_Speed speed;
  GPIO_Pull pull;

} GPIO_Config;

static inline volatile uint32_t *gpio_cr_reg(GPIO_TypeDef *port, uint8_t pin) {
  return (pin < 8) ? &port->CRL : &port->CRH;
}

static inline uint8_t gpio_cr_shift(uint8_t pin) { return (pin % 8) * 4; }

uint8_t gpio_encode_config(GPIO_Mode mode, GPIO_Speed speed, GPIO_Pull pull);

void hal_gpio_pin_init(GPIO_TypeDef *port, uint8_t pin, const GPIO_Config *cfg);
void hal_gpio_pin_deinit(GPIO_TypeDef *port, uint8_t pin);

void hal_gpio_pin_lock(GPIO_TypeDef *port, uint8_t pin);
void hal_gpio_alternate_en(GPIO_TypeDef *port, uint8_t pin);

static inline void hal_gpio_port_write(GPIO_TypeDef *port, uint16_t mask) {
  port->ODR = mask;
};
static inline uint16_t hal_gpio_port_read(GPIO_TypeDef *port) {
  return (uint16_t)port->IDR;
};

static inline void hal_gpio_pin_write(GPIO_TypeDef *port, uint8_t pin,
                                      bool level) {
  if (level) {
    port->BSRR = (1U << pin);
  } else {
    port->BRR = (1U << pin);
  }
};
static inline bool hal_gpio_pin_read(GPIO_TypeDef *port, uint8_t pin) {
  return (port->IDR & (1U << pin)) ? true : false;
};

static inline void hal_gpio_pin_toggle(GPIO_TypeDef *port, uint8_t pin) {
  port->BSRR = ((port->ODR & (1U << pin)) ? (1U << (pin + 16)) : (1U << pin));
};

#endif /* HAL_GPIO_H */
