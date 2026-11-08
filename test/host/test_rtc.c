/**
 * @file test_rtc.c
 * @brief Host-side unit tests for the F1 RTC register sequences
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Runs on an RTC_TypeDef in host RAM with RTOFF preset, so the "previous
 * write finished" waits pass at once.
 */

#include "hal_rtc.h"
#include "test_assert.h"

void test_rtc_set_get(void) {
  RTC_TypeDef r = {0};
  r.CRL = RTC_CRL_RTOFF;

  ASSERT_EQ(hal_rtc_set(&r, 0x00012345U), HAL_OK);
  ASSERT_EQ(r.CNTH, 0x0001U); /* 32-bit counter split in two 16-bit halves */
  ASSERT_EQ(r.CNTL, 0x2345U);
  ASSERT_EQ(r.CRL & RTC_CRL_CNF, 0U); /* left configuration mode */
  ASSERT_EQ(hal_rtc_get(&r), 0x00012345U);
}

void test_rtc_alarm_at(void) {
  RTC_TypeDef r = {0};
  r.CRL = RTC_CRL_RTOFF | RTC_CRL_ALRF; /* stale alarm from before */

  ASSERT_EQ(hal_rtc_alarm_at(&r, 0x0002000AU), HAL_OK);
  ASSERT_EQ(r.ALRH, 0x0002U);
  ASSERT_EQ(r.ALRL, 0x000AU);
  ASSERT_EQ(r.CRL & RTC_CRL_ALRF, 0U); /* cleared so EXTI 17 sees a new edge */
  ASSERT_EQ(r.CRH & RTC_CRH_ALRIE, RTC_CRH_ALRIE);
}

void test_rtc_alarm_fired_once(void) {
  RTC_TypeDef r = {0};
  r.CRL = RTC_CRL_RTOFF | RTC_CRL_ALRF;

  ASSERT_TRUE(hal_rtc_alarm_fired(&r));
  ASSERT_EQ(r.CRL & RTC_CRL_ALRF, 0U);
  ASSERT_TRUE(!hal_rtc_alarm_fired(&r));
}
