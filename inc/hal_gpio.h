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
#include <stdint.h>

// GPIO Modes
typedef enum {
  GPIO_MODE_INPUT_FLOATING,
  GPIO_MODE_INPUT_PULLUP,
  GPIO_MODE_OUTPUT_PP,
  GPIO_MODE_OUTPUT_OD,
  GPIO_MODE_AF_PP,
  GPIO_MODE_AF_OD,
  GPIO_MODE_ANALOG
} GPIO_Mode;

// TODO:
typedef struct {

} GPIO_Speed;
// TODO:
typedef struct {

} GPIO_Pull;

typedef struct {
  GPIO_Mode mode;
  GPIO_Speed speed;
  GPIO_Pull pull;

} GPIO_Config;

static const uint8_t gpio_mode_bits[] = {
    [GPIO_MODE_INPUT_FLOATING] = 0x4, [GPIO_MODE_INPUT_PULLUP] = 0x8,
    [GPIO_MODE_OUTPUT_PP] = 0x2,      [GPIO_MODE_OUTPUT_OD] = 0x6,
    [GPIO_MODE_AF_PP] = 0xA,          [GPIO_MODE_AF_OD] = 0xE,
    [GPIO_MODE_ANALOG] = 0x0,
};

void hal_gpio_pin_init(GPIO_TypeDef *port, uint8_t pin, GPIO_Config);
void hal_gpio_pin_deinit(GPIO_TypeDef *port, uint8_t pin, GPIO_Config);
void hal_gpio_pin_lock(GPIO_TypeDef *port, uint8_t pin);

static inline void hal_gpio_write_port(GPIO_TypeDef *port, uint16_t mask);
static inline void hal_gpio_read_port(GPIO_TypeDef *port);

static inline void hal_gpio_write(GPIO_TypeDef *port, uint8_t pin, _Bool level);
static inline void hal_gpio_read(GPIO_TypeDef *port, uint8_t pin);
static inline void hal_gpio_toggle(GPIO_TypeDef *port, uint8_t pin);

#endif /* HAL_GPIO_H */
