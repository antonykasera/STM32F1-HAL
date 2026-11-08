/**
 * @file hal_rtc_f1.c
 * @brief STM32F1 RTC and backup registers
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_rtc.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define RTC_LSE_HZ 32768U
#define RTC_LSI_HZ 40000U
#define RTC_WRITE_TIMEOUT_MS 10U /* >= 3 RTC clocks, ~100 us at 30 kHz */
#define RTC_EXTI_LINE (1UL << 17)
#define RTC_BACKUP_REGS 10U

/* CRL flags are rc_w0: writing 1 leaves them, so writes carry them as 1s.
 * RTOFF is read-only; writing it as 1 is ignored by the hardware. */
#define RTC_CRL_KEEP                                                           \
  (RTC_CRL_RTOFF | RTC_CRL_RSF | RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF)

static HAL_Status rtc_wait_crl(const RTC_TypeDef *rtc, uint32_t bit,
                               uint32_t timeout_ms) {
  uint32_t start = hal_get_tick();

  while ((rtc->CRL & bit) == 0U) {
    if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return HAL_OK;
}

/* RM0008 18.3.4: wait RTOFF, CNF on, write, CNF off, wait RTOFF */
static HAL_Status rtc_config_begin(RTC_TypeDef *rtc) {
  HAL_Status st = rtc_wait_crl(rtc, RTC_CRL_RTOFF, RTC_WRITE_TIMEOUT_MS);
  if (st == HAL_OK) {
    rtc->CRL = RTC_CRL_KEEP | RTC_CRL_CNF;
  }
  return st;
}

static HAL_Status rtc_config_end(RTC_TypeDef *rtc) {
  rtc->CRL = RTC_CRL_KEEP; /* CNF off: the write executes now */
  return rtc_wait_crl(rtc, RTC_CRL_RTOFF, RTC_WRITE_TIMEOUT_MS);
}

/* after reset/Stop/Standby the first reads can be stale until RSF (18.3.3) */
static HAL_Status rtc_sync(RTC_TypeDef *rtc) {
  rtc->CRL = RTC_CRL_KEEP & ~RTC_CRL_RSF;
  return rtc_wait_crl(rtc, RTC_CRL_RSF, RTC_WRITE_TIMEOUT_MS);
}

HAL_Status hal_rtc_init(RTC_TypeDef *rtc, RTC_Clock src, uint32_t timeout_ms) {
  HAL_ASSERT(rtc == RTC);

  RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
  PWR->CR |= PWR_CR_DBP; /* the backup domain is write-protected until this */

  uint32_t sel = (src == RTC_CLOCK_LSE) ? RCC_BDCR_RTCSEL_LSE
                                        : RCC_BDCR_RTCSEL_LSI;
  uint32_t hz = (src == RTC_CLOCK_LSE) ? RTC_LSE_HZ : RTC_LSI_HZ;
  uint32_t start = hal_get_tick();

  if (src == RTC_CLOCK_LSI) {
    RCC->CSR |= RCC_CSR_LSION; /* LSI is outside the backup domain: off after reset */
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U) {
      if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
        return HAL_ERR_TIMEOUT;
      }
    }
  }

  uint32_t bdcr = RCC->BDCR;
  if (((bdcr & RCC_BDCR_RTCEN) != 0U) && ((bdcr & RCC_BDCR_RTCSEL) == sel)) {
    return rtc_sync(rtc); /* already counting on this clock: keep the time */
  }

  if ((bdcr & RCC_BDCR_RTCSEL) != 0U) {
    /* RTCSEL only changes after a backup domain reset */
    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;
  }

  if (src == RTC_CLOCK_LSE) {
    RCC->BDCR |= RCC_BDCR_LSEON;
    while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0U) {
      if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
        return HAL_ERR_TIMEOUT; /* no crystal, or it hasn't started */
      }
    }
  }
  RCC->BDCR |= sel;
  RCC->BDCR |= RCC_BDCR_RTCEN;

  HAL_Status st = rtc_sync(rtc);
  if (st != HAL_OK) {
    return st;
  }
  st = rtc_config_begin(rtc);
  if (st != HAL_OK) {
    return st;
  }
  rtc->PRLH = (hz - 1U) >> 16;
  rtc->PRLL = (hz - 1U) & 0xFFFFU;
  rtc->CNTH = 0U;
  rtc->CNTL = 0U;
  return rtc_config_end(rtc);
}

uint32_t hal_rtc_get(const RTC_TypeDef *rtc) {
  uint32_t hi = rtc->CNTH & 0xFFFFU;
  uint32_t lo = rtc->CNTL & 0xFFFFU;
  uint32_t hi2 = rtc->CNTH & 0xFFFFU;

  if (hi != hi2) { /* low half wrapped between the reads */
    lo = rtc->CNTL & 0xFFFFU;
    hi = hi2;
  }
  return (hi << 16) | lo;
}

HAL_Status hal_rtc_set(RTC_TypeDef *rtc, uint32_t seconds) {
  HAL_Status st = rtc_config_begin(rtc);
  if (st != HAL_OK) {
    return st;
  }
  rtc->CNTH = seconds >> 16;
  rtc->CNTL = seconds & 0xFFFFU;
  return rtc_config_end(rtc);
}

HAL_Status hal_rtc_alarm_at(RTC_TypeDef *rtc, uint32_t seconds) {
  HAL_Status st = rtc_config_begin(rtc);
  if (st != HAL_OK) {
    return st;
  }
  rtc->ALRH = seconds >> 16;
  rtc->ALRL = seconds & 0xFFFFU;
  st = rtc_config_end(rtc);

  /* a stale ALRF would hide the next rising edge EXTI 17 needs */
  rtc->CRL = RTC_CRL_KEEP & ~RTC_CRL_ALRF;
  rtc->CRH |= RTC_CRH_ALRIE;
  return st;
}

bool hal_rtc_alarm_fired(RTC_TypeDef *rtc) {
  if ((rtc->CRL & RTC_CRL_ALRF) == 0U) {
    return false;
  }
  rtc->CRL = RTC_CRL_KEEP & ~RTC_CRL_ALRF;
  return true;
}

void hal_rtc_alarm_wakes_stop(void) {
  EXTI->RTSR |= RTC_EXTI_LINE;
  EXTI->IMR |= RTC_EXTI_LINE;
  __NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

/* only needs to end the EXTI request; ALRF stays set for alarm_fired */
void RTC_Alarm_IRQHandler(void) { EXTI->PR = RTC_EXTI_LINE; }

uint16_t hal_rtc_backup_read(uint8_t n) {
  HAL_ASSERT((n >= 1U) && (n <= RTC_BACKUP_REGS));
  return (uint16_t)((&BKP->DR1)[n - 1U] & 0xFFFFU);
}

void hal_rtc_backup_write(uint8_t n, uint16_t value) {
  HAL_ASSERT((n >= 1U) && (n <= RTC_BACKUP_REGS));
  (&BKP->DR1)[n - 1U] = value;
}
