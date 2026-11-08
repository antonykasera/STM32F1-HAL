/**
 * @file test_clock.c
 * @brief Host-side unit tests for the F1 clock math
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT

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

  /* HSI reaches the PLL div by 2 (RCC_CFGR.PLLSRC = 0): 4 MHz * 2 = 8 MHz */
  Clock_Config c8_hsi = {CLOCK_SOURCE_HSI, 2, CLOCK_AHB_DIV1, CLOCK_APB_DIV1,
                         CLOCK_APB_DIV1};
  ASSERT_EQ(hal_clock_sysclk_for(&c8_hsi), 8000000U);

  /* highest clock available from HSI: 4 MHz * 16 = 64 MHz */
  Clock_Config c64_hsi = {CLOCK_SOURCE_HSI, 16, CLOCK_AHB_DIV1, CLOCK_APB_DIV2,
                          CLOCK_APB_DIV1};
  ASSERT_EQ(hal_clock_sysclk_for(&c64_hsi), 64000000U);
}

void test_default_config_picks_2ws(void) {
  /* default cfg -> 72 MHz -> 2 wait states */
  uint32_t sysclk = hal_clock_sysclk_for(&CLOCK_CONFIG_72MHZ_HSE);
  ASSERT_EQ(hal_clock_flash_latency_for(sysclk), FLASH_ACR_LATENCY_2);
}

void test_tick_expired(void) {
  /* waits at least the full timeout */
  ASSERT_TRUE(!hal_tick_expired(100U, 100U, 5U));
  ASSERT_TRUE(!hal_tick_expired(105U, 100U, 5U));
  ASSERT_TRUE(hal_tick_expired(106U, 100U, 5U));

  /* still correct when the 32-bit tick wraps past zero */
  ASSERT_TRUE(!hal_tick_expired(3U, 0xFFFFFFFEU, 5U));
  ASSERT_TRUE(hal_tick_expired(4U, 0xFFFFFFFEU, 5U));
}
