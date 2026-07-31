/**
 * @file test_clock.c
 * @brief Host-side unit tests for the F1 clock math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Only the PURE helpers are exercised here -- the ones that compute
 * frequencies and derive flash wait-states from a Clock_Config. The register
 * functions (hal_sysclock_init) dereference RCC/FLASH at fixed addresses and
 * are NOT host-callable, so they are deliberately untested here.
 *
 * Flash-latency thresholds cross-checked against RM0008 Rev 21, Flash ACR
 * (LATENCY): 0WS <= 24 MHz, 1WS <= 48 MHz, 2WS <= 72 MHz.
 */

#include "hal_clock.h"
#include "stm32f103xb.h"
#include "test_assert.h"

void test_flash_latency_boundaries(void) {
  /* 0 wait states up to and including 24 MHz */
  ASSERT_EQ(hal_clock_flash_latency_for(8000000U), FLASH_ACR_LATENCY_0);
  ASSERT_EQ(hal_clock_flash_latency_for(24000000U), FLASH_ACR_LATENCY_0);

  /* 1 wait state above 24 up to and including 48 MHz */
  ASSERT_EQ(hal_clock_flash_latency_for(24000001U), FLASH_ACR_LATENCY_1);
  ASSERT_EQ(hal_clock_flash_latency_for(48000000U), FLASH_ACR_LATENCY_1);

  /* 2 wait states above 48 MHz */
  ASSERT_EQ(hal_clock_flash_latency_for(48000001U), FLASH_ACR_LATENCY_2);
  ASSERT_EQ(hal_clock_flash_latency_for(72000000U), FLASH_ACR_LATENCY_2);
}

void test_sysclk_from_config(void) {
  /* shipped default: HSE 8 MHz * PLL 9 = 72 MHz */
  ASSERT_EQ(hal_clock_sysclk_for(&CLOCK_CONFIG_72MHZ_HSE), 72000000U);

  Clock_Config c48 = {CLOCK_SOURCE_HSE, 6, CLOCK_AHB_DIV1, CLOCK_APB_DIV2,
                      CLOCK_APB_DIV1};
  ASSERT_EQ(hal_clock_sysclk_for(&c48), 48000000U);

  Clock_Config c16_hsi = {CLOCK_SOURCE_HSI, 2, CLOCK_AHB_DIV1, CLOCK_APB_DIV1,
                          CLOCK_APB_DIV1};
  ASSERT_EQ(hal_clock_sysclk_for(&c16_hsi), 16000000U);
}

void test_default_config_picks_2ws(void) {
  /* default cfg -> 72 MHz -> 2 wait states */
  uint32_t sysclk = hal_clock_sysclk_for(&CLOCK_CONFIG_72MHZ_HSE);
  ASSERT_EQ(hal_clock_flash_latency_for(sysclk), FLASH_ACR_LATENCY_2);
}
