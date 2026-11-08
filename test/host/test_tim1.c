/**
 * @file test_tim1.c
 * @brief Host-side unit tests for TIM1 dead time, BDTR and output control
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Values at 72 MHz (13.9 ns per tick) cross-checked against an independent
 * model of the RM0008 BDTR.DTG table.
 */

#include "hal_tim1.h"
#include "test_assert.h"

/* RM0008 BDTR.DTG decode, in timer ticks */
static uint32_t dtg_ticks(uint32_t c) {
  if ((c & 0x80U) == 0U) {
    return c;
  }
  if ((c & 0xC0U) == 0x80U) {
    return (64U + (c & 0x3FU)) * 2U;
  }
  if ((c & 0xE0U) == 0xC0U) {
    return (32U + (c & 0x1FU)) * 8U;
  }
  return (32U + (c & 0x1FU)) * 16U;
}

static void check_dtg(uint32_t ns, uint8_t want_dtg, uint32_t want_ns) {
  uint8_t dtg = 0U;
  uint32_t actual = 0U;

  ASSERT_EQ(hal_tim1_calc_dtg(72000000U, ns, &dtg, &actual), HAL_OK);
  ASSERT_EQ(dtg, want_dtg);
  ASSERT_EQ(actual, want_ns);
}

void test_tim1_dtg_values(void) {
  check_dtg(0U, 0x00U, 0U);
  check_dtg(100U, 0x08U, 111U); /* 7.2 ticks rounds UP to 8 */
  check_dtg(500U, 0x24U, 500U);
  check_dtg(1000U, 0x48U, 1000U);
  check_dtg(2000U, 0x88U, 2000U);   /* 2-tick steps */
  check_dtg(5000U, 0xCDU, 5000U);   /* 8-tick steps */
  check_dtg(10000U, 0xEDU, 10000U); /* 16-tick steps */
  check_dtg(14000U, 0xFFU, 14000U); /* 1008 ticks, the maximum */
}

void test_tim1_dtg_rejects(void) {
  uint8_t dtg = 0x77U;
  uint32_t actual = 7U;

  ASSERT_EQ(hal_tim1_calc_dtg(72000000U, 15000U, &dtg, &actual),
            HAL_ERR_PARAM);
  ASSERT_EQ(dtg, 0x77U);
}

void test_tim1_dtg_shortest_safe(void) {
  /* at 1 GHz one tick is 1 ns: for every request, the code must give at least
   * that much dead time and no other code may give less while still enough */
  uint32_t bad = 0U;
  for (uint32_t want = 0U; want <= 1008U; want++) {
    uint8_t dtg = 0U;
    uint32_t actual = 0U;
    if (hal_tim1_calc_dtg(1000000000U, want, &dtg, &actual) != HAL_OK) {
      bad++;
      continue;
    }
    uint32_t got = dtg_ticks(dtg);
    uint32_t best = 0xFFFFFFFFU;
    for (uint32_t c = 0U; c < 256U; c++) {
      uint32_t t = dtg_ticks(c);
      if ((t >= want) && (t < best)) {
        best = t;
      }
    }
    if ((got < want) || (got != best) || (actual != got)) {
      bad++;
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_tim1_bdtr(void) {
  /* DTG | OSSI 0x400 | OSSR 0x800 | BKE 0x1000, BKP 0 = active low, no MOE */
  ASSERT_EQ(hal_tim1_calc_bdtr(0x48U, true), 0x1C48U);
  ASSERT_EQ(hal_tim1_calc_bdtr(0x88U, false), 0x0C88U);
}

/* output control on a TIM_TypeDef in host RAM */

void test_tim1_channel_complementary(void) {
  TIM_TypeDef t = {0};

  hal_tim1_channel_init(&t, PWM_CH2, true);
  ASSERT_EQ(t.CCER, TIM_CCER_CC2E | TIM_CCER_CC2NE);

  hal_tim1_channel_init(&t, PWM_CH1, false);
  ASSERT_EQ(t.CCER & (TIM_CCER_CC1E | TIM_CCER_CC1NE), TIM_CCER_CC1E);
}

void test_tim1_start_stop_trip(void) {
  TIM_TypeDef t = {0};

  t.SR = TIM_SR_BIF; /* a break happened */
  ASSERT_TRUE(hal_tim1_tripped(&t));

  hal_tim1_start(&t);
  ASSERT_EQ(t.SR & TIM_SR_BIF, 0U); /* cleared on (re)start */
  ASSERT_EQ(t.BDTR & TIM_BDTR_MOE, TIM_BDTR_MOE);
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, TIM_CR1_CEN);
  ASSERT_TRUE(!hal_tim1_tripped(&t));

  hal_tim1_stop(&t);
  ASSERT_EQ(t.BDTR & TIM_BDTR_MOE, 0U); /* outputs to idle (low) */
  ASSERT_EQ(t.CR1 & TIM_CR1_CEN, 0U);
}
