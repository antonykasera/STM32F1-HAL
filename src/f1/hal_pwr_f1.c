/**
 * @file hal_pwr_f1.c
 * @brief STM32F1 low-power modes
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_clock.h"
#include "hal_pwr.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>

static void pwr_clock_on(void) { RCC->APB1ENR |= RCC_APB1ENR_PWREN; }

void hal_pwr_sleep(void) { __WFI(); }

void hal_pwr_stop(const Clock_Config *restore) {
  pwr_clock_on();
  PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS; /* Stop, regulator low-power */

  /* a pending SysTick would make WFI return at once instead of stopping */
  uint32_t systick = SysTick->CTRL;
  SysTick->CTRL = systick & ~SysTick_CTRL_ENABLE_Msk;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __WFI();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

  if (restore != NULL) {
    hal_sysclock_init(restore); /* Stop exits on HSI (RM0008) */
  }
  SysTick->CTRL = systick;
}

void hal_pwr_standby(void) {
  pwr_clock_on();
  SysTick->CTRL = 0U;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  PWR->CR |= PWR_CR_PDDS | PWR_CR_CWUF; /* a set WUF would exit at once */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  for (;;) {
    __WFI();
  }
}

bool hal_pwr_woke_from_standby(void) {
  pwr_clock_on();
  bool sb = (PWR->CSR & PWR_CSR_SBF) != 0U;
  PWR->CR |= PWR_CR_CSBF;
  return sb;
}

void hal_pwr_wakeup_pin(bool enable) {
  pwr_clock_on();
  if (enable) {
    PWR->CSR |= PWR_CSR_EWUP;
  } else {
    PWR->CSR &= ~PWR_CSR_EWUP;
  }
}

void hal_pwr_debug_keepalive(bool enable) {
  uint32_t bits =
      DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;
  if (enable) {
    DBGMCU->CR |= bits;
  } else {
    DBGMCU->CR &= ~bits;
  }
}
