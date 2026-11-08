/**
 * @file hal_pwr.h
 * @brief Low-power modes: Sleep, Stop, Standby
 *
 *   Sleep:   CPU off, everything else running; any interrupt wakes it
 *            (SysTick does every 1 ms).
 *   Stop:    all clocks off, RAM and registers kept; an EXTI line wakes it
 *            (RTC alarm via hal_rtc_alarm_wakes_stop). Wakes on HSI 8 MHz,
 *            so the clock tree is restored afterwards. SysTick doesn't count.
 *   Standby: almost everything off, RAM lost; wakes by reset (RTC alarm,
 *            WKUP pin PA0, NRST). Backup registers and the RTC survive.
 *
 * A debugger loses the chip in Stop/Standby unless hal_pwr_debug_keepalive
 * is on (which keeps clocks running, so current readings are meaningless).
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_PWR_H
#define HAL_PWR_H

#include "hal_clock.h"
#include "stm32f103xb.h"
#include <stdbool.h>

void hal_pwr_sleep(void);
/* returns after wake-up with `restore` reapplied (NULL: stay on HSI 8 MHz) */
void hal_pwr_stop(const Clock_Config *restore);
/* does not return */
void hal_pwr_standby(void);

/* true if this boot came from Standby; clears the flag */
bool hal_pwr_woke_from_standby(void);
/* PA0 rising edge wakes Standby */
void hal_pwr_wakeup_pin(bool enable);
void hal_pwr_debug_keepalive(bool enable);

#endif /* HAL_PWR_H */
