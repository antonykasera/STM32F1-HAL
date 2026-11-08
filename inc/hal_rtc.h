/**
 * @file hal_rtc.h
 * @brief RTC: 1 Hz seconds counter and alarm, plus the backup registers
 *
 * The RTC and backup registers live in the backup domain: they keep running
 * and keep their values through reset and Standby (and on VBAT). LSE is the
 * 32.768 kHz crystal on PC14/PC15 (accurate); LSI is the ~40 kHz internal RC,
 * 30-60 kHz per the datasheet, so seconds can be +-50% off.
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_RTC_H
#define HAL_RTC_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { RTC_CLOCK_LSE, RTC_CLOCK_LSI } RTC_Clock;

/* 1 Hz counter. If already running on this clock (after reset/Standby) the
 * time is kept. LSE can take ~1-2 s to start: timeout_ms covers it. Changing
 * the clock source resets the backup domain, backup registers included. */
HAL_Status hal_rtc_init(RTC_TypeDef *rtc, RTC_Clock src, uint32_t timeout_ms);

uint32_t hal_rtc_get(const RTC_TypeDef *rtc);
HAL_Status hal_rtc_set(RTC_TypeDef *rtc, uint32_t seconds);

/* alarm when the counter reaches `seconds`; also wakes Standby */
HAL_Status hal_rtc_alarm_at(RTC_TypeDef *rtc, uint32_t seconds);
/* true once after the alarm fired (clears it) */
bool hal_rtc_alarm_fired(RTC_TypeDef *rtc);
/* route the alarm to EXTI line 17 so it can also wake Stop mode */
void hal_rtc_alarm_wakes_stop(void);

/* backup data registers DR1..DR10, 16 bits each */
uint16_t hal_rtc_backup_read(uint8_t n);
void hal_rtc_backup_write(uint8_t n, uint16_t value);

#endif /* HAL_RTC_H */
