/**
 * @file test_iwdg.c
 * @brief Host-side unit tests for the F1 IWDG timeout math and start sequence
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * timeout_ms = (4 << PR) * (RLR + 1) / 40 at the nominal 40 kHz LSI.
 */

#include "hal_iwdg.h"
#include "test_assert.h"

static void check_calc(uint32_t ms, uint8_t pr, uint16_t rlr) {
  uint8_t got_pr = 0xFFU;
  uint16_t got_rlr = 0xFFFFU;

  ASSERT_EQ(hal_iwdg_calc(ms, &got_pr, &got_rlr), HAL_OK);
  ASSERT_EQ(got_pr, pr);
  ASSERT_EQ(got_rlr, rlr);
}

void test_iwdg_calc_values(void) {
  check_calc(1U, 0U, 9U);         /* /4: 10 counts of 0.1 ms */
  check_calc(100U, 0U, 999U);
  check_calc(409U, 0U, 4089U);    /* last that fits /4 */
  check_calc(410U, 1U, 2049U);    /* 4100 counts: needs /8 */
  check_calc(1000U, 2U, 2499U);   /* /16 */
  check_calc(26214U, 6U, 4094U);  /* /256, near the top */
}

void test_iwdg_calc_rejects(void) {
  uint8_t pr = 0x77U;
  uint16_t rlr = 0x7777U;

  ASSERT_EQ(hal_iwdg_calc(0U, &pr, &rlr), HAL_ERR_PARAM);
  ASSERT_EQ(hal_iwdg_calc(30000U, &pr, &rlr), HAL_ERR_PARAM); /* > ~26.2 s */
  ASSERT_EQ(hal_iwdg_calc(100U, NULL, &rlr), HAL_ERR_PARAM);
  ASSERT_EQ(pr, 0x77U); /* untouched on error */
  ASSERT_EQ(rlr, 0x7777U);
}

/* register-level, on an IWDG_TypeDef in host RAM */

void test_iwdg_start_and_feed(void) {
  IWDG_TypeDef w = {0};

  ASSERT_EQ(hal_iwdg_start(&w, 1000U), HAL_OK);
  ASSERT_EQ(w.PR, 2U);
  ASSERT_EQ(w.RLR, 2499U);
  ASSERT_EQ(w.KR, 0xAAAAU); /* last key written: reload */

  w.KR = 0U;
  hal_iwdg_feed(&w);
  ASSERT_EQ(w.KR, 0xAAAAU);
}
