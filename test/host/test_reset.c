/**
 * @file test_reset.c
 * @brief Host-side unit tests for reset cause decoding
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * RCC_CSR reset flags: PINRSTF bit 26 .. LPWRRSTF bit 31 (RM0008 7.3.10).
 */

#include "hal_reset.h"
#include "stm32f103xb.h"
#include "test_assert.h"
#include <string.h>

/* stand-in: hal_reset_f1.c uses NVIC_SystemReset (ARM-only asm) so can't
 * build on host; hal_iwdg_f1.c and hal_wwdg_f1.c link against this */
uint32_t hal_reset_cause(void) { return 0U; }

void test_reset_calc_cause(void) {
  ASSERT_EQ(hal_reset_calc_cause(0U), 0U);
  /* CSR reset value after a power reset: POR | PIN */
  ASSERT_EQ(hal_reset_calc_cause(0x0C000000U),
            (uint32_t)(HAL_RESET_POWER_ON | HAL_RESET_PIN));
  ASSERT_EQ(hal_reset_calc_cause(RCC_CSR_IWDGRSTF | RCC_CSR_PINRSTF),
            (uint32_t)(HAL_RESET_IWDG | HAL_RESET_PIN));
  ASSERT_EQ(hal_reset_calc_cause(RCC_CSR_WWDGRSTF), (uint32_t)HAL_RESET_WWDG);
  ASSERT_EQ(hal_reset_calc_cause(RCC_CSR_SFTRSTF), (uint32_t)HAL_RESET_SOFTWARE);
  ASSERT_EQ(hal_reset_calc_cause(RCC_CSR_LPWRRSTF),
            (uint32_t)HAL_RESET_LOW_POWER);
  /* RMVF, LSIRDY, LSION and reserved bits are not causes */
  ASSERT_EQ(hal_reset_calc_cause(RCC_CSR_RMVF | RCC_CSR_LSIRDY | RCC_CSR_LSION |
                                 (1UL << 25)),
            0U);
}

void test_reset_cause_str(void) {
  ASSERT_TRUE(strcmp(hal_reset_cause_str(0U), "none") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_PIN), "pin") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_POWER_ON | HAL_RESET_PIN),
                     "power-on") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_SOFTWARE | HAL_RESET_PIN),
                     "software") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_IWDG | HAL_RESET_PIN),
                     "iwdg") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_WWDG | HAL_RESET_PIN),
                     "wwdg") == 0);
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_LOW_POWER | HAL_RESET_PIN),
                     "low-power") == 0);
  /* flags left over from an earlier boot: the most specific wins */
  ASSERT_TRUE(strcmp(hal_reset_cause_str(HAL_RESET_IWDG | HAL_RESET_POWER_ON |
                                         HAL_RESET_PIN),
                     "iwdg") == 0);
}
