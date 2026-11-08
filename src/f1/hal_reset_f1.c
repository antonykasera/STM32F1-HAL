/**
 * @file hal_reset_f1.c
 * @brief STM32F1 reset cause and software reset
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_reset.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

/* .bss is zeroed on every reset, so this is per boot */
static bool cause_read;
static uint32_t cause;

uint32_t hal_reset_cause(void) {
  if (!cause_read) {
    cause = hal_reset_calc_cause(RCC->CSR);
    RCC->CSR |= RCC_CSR_RMVF;
    cause_read = true;
  }
  return cause;
}

void hal_reset_software(void) { NVIC_SystemReset(); }
