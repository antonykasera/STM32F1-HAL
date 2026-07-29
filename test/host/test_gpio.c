/**
 * @file test_gpio.c
 * @brief Host-side unit tests for the F1 GPIO driver
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Strategy: the HAL takes a `GPIO_TypeDef *`, so we allocate the peripheral
 * struct in ordinary host RAM, pass its address in, then inspect the register
 * fields directly. No target hardware, no fixed-address dereference.
 *
 * Expected nibble values are cross-checked against RM0008 Rev 21, Table 20
 * (Port bit configuration) and Table 21 (Output MODE bits).
 */

#include "hal_gpio.h"
#include "stm32f103xb.h"
#include "test_assert.h"

static uint8_t nibble_of(const GPIO_TypeDef *g, uint8_t pin) {
  uint32_t reg = (pin < 8) ? g->CRL : g->CRH;
  return (uint8_t)((reg >> ((pin % 8) * 4)) & 0xF);
}

static void test_encode_output_modes(void) {
  ASSERT_EQ(gpio_encode_config(GPIO_MODE_OUTPUT_PP, GPIO_SPEED_10_MHZ,
                               GPIO_PULL_NONE),
            0b0001);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE),
      0b0010);
  ASSERT_EQ(gpio_encode_config(GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50_MHZ,
                               GPIO_PULL_NONE),
            0b0011);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_OUTPUT_OD, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE),
      0b0110);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_AF_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE),
      0b1011);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_AF_OD, GPIO_SPEED_10_MHZ, GPIO_PULL_NONE),
      0b1101);
}

static void test_encode_input_modes(void) {
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_ANALOG, GPIO_SPEED_INPUT, GPIO_PULL_NONE),
      0b0000);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE),
      0b0100);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP),
      0b1000);
  ASSERT_EQ(gpio_encode_config(GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT,
                               GPIO_PULL_DOWN),
            0b1000);
  ASSERT_EQ(
      gpio_encode_config(GPIO_MODE_INPUT, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE),
      0b0100);
}

static void test_cr_shift(void) {
  ASSERT_EQ(gpio_cr_shift(0), 0);
  ASSERT_EQ(gpio_cr_shift(7), 28);
  ASSERT_EQ(gpio_cr_shift(8), 0);
  ASSERT_EQ(gpio_cr_shift(13), 20);
  ASSERT_EQ(gpio_cr_shift(15), 60 - 32);
}

static void test_pin_init_pc13(void) {
  GPIO_TypeDef g = {0};
  GPIO_Config cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  hal_gpio_pin_init(&g, 13, &cfg);

  ASSERT_EQ(nibble_of(&g, 13), 0b0010);
  ASSERT_EQ(g.CRL, 0);
}

static void test_pin_init_crl_isolation(void) {
  GPIO_TypeDef g = {0};
  GPIO_Config cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50_MHZ, GPIO_PULL_NONE};

  hal_gpio_pin_init(&g, 5, &cfg);

  ASSERT_EQ(nibble_of(&g, 5), 0b0011);
  ASSERT_EQ(g.CRH, 0);

  ASSERT_EQ(g.CRL & ~(0xFu << 20), 0);
}

static void test_pin_init_pull_direction(void) {
  GPIO_TypeDef up = {0}, down = {0};
  GPIO_Config cfg_up = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_UP};
  GPIO_Config cfg_dn = {GPIO_MODE_INPUT_PULL, GPIO_SPEED_INPUT, GPIO_PULL_DOWN};

  hal_gpio_pin_init(&up, 4, &cfg_up);
  hal_gpio_pin_init(&down, 4, &cfg_dn);

  ASSERT_EQ(nibble_of(&up, 4), 0b1000);
  ASSERT_EQ(up.BSRR, (1u << 4));
  ASSERT_EQ(down.BRR, (1u << 4));
}

static void test_write_and_toggle(void) {
  GPIO_TypeDef g = {0};

  hal_gpio_pin_write(&g, 13, true);
  ASSERT_EQ(g.BSRR, (1u << 13));

  g.BRR = 0;
  hal_gpio_pin_write(&g, 13, false);
  ASSERT_EQ(g.BRR, (1u << 13));

  g.ODR = (1u << 9);
  g.BSRR = 0;
  hal_gpio_pin_toggle(&g, 9);
  ASSERT_EQ(g.BSRR, (1u << (9 + 16)));

  g.ODR = 0;
  g.BSRR = 0;
  hal_gpio_pin_toggle(&g, 9);
  ASSERT_EQ(g.BSRR, (1u << 9));
}

int main(void) {
  TEST_RUN(test_encode_output_modes);
  TEST_RUN(test_encode_input_modes);
  TEST_RUN(test_cr_shift);
  TEST_RUN(test_pin_init_pc13);
  TEST_RUN(test_pin_init_crl_isolation);
  TEST_RUN(test_pin_init_pull_direction);
  TEST_RUN(test_write_and_toggle);
  return TEST_SUMMARY();
}
