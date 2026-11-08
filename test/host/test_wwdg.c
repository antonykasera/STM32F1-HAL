/**
 * @file test_wwdg.c
 * @brief Host-side unit tests for the F1 WWDG timeout/window math
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * At PCLK1 = 36 MHz one tick is 113.78 us << WDGTB.
 */

#include "hal_wwdg.h"
#include "test_assert.h"

#define PCLK1 36000000U

static void check_calc(uint32_t min_us, uint32_t max_us, uint32_t cfr,
                       uint8_t cr) {
  uint32_t got_cfr = 0xFFFFU;
  uint8_t got_cr = 0xFFU;

  ASSERT_EQ(hal_wwdg_calc(PCLK1, min_us, max_us, &got_cfr, &got_cr), HAL_OK);
  ASSERT_EQ(got_cfr, cfr);
  ASSERT_EQ(got_cr, cr);
}

void test_wwdg_calc_values(void) {
  check_calc(0U, 7282U, 0x07FU, 0xFFU);   /* 64 ticks /1, no window */
  check_calc(0U, 7400U, 0x0DFU, 0xDFU);   /* 65 ticks: /2, 32 of them */
  check_calc(0U, 114U, 0x040U, 0xC0U);    /* a single tick */
  check_calc(20000U, 50000U, 0x1E0U, 0xF5U); /* /8: T 0x75, opens 21 ticks in */
  check_calc(1000U, 1200U, 0x041U, 0xC9U); /* /1: T 0x49, W 0x49 - 8 */
}

void test_wwdg_calc_rejects(void) {
  uint32_t cfr = 0x77U;
  uint8_t cr = 0x77U;

  ASSERT_EQ(hal_wwdg_calc(PCLK1, 0U, 60000U, &cfr, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_wwdg_calc(PCLK1, 0U, 100U, &cfr, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_wwdg_calc(PCLK1, 5000U, 5000U, &cfr, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_wwdg_calc(PCLK1, 49500U, 50000U, &cfr, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_wwdg_calc(0U, 0U, 1000U, &cfr, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_wwdg_calc(PCLK1, 0U, 1000U, NULL, &cr), HAL_ERR_PARAM);
  ASSERT_EQ(cfr, 0x77U); /* untouched on error */
  ASSERT_EQ(cr, 0x77U);
}
