/**
 * @file hal_dma_f1.c
 * @brief STM32F1 DMA1 driver
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "hal_dma.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define DMA_FLAG_BITS 4U /* GIF, TCIF, HTIF, TEIF per channel */

static uint32_t dma_flag(const DMA_Channel_TypeDef *ch, uint32_t ch1_flag) {
  return ch1_flag << (DMA_FLAG_BITS * hal_dma_channel_index(ch));
}

void hal_dma_start(DMA_Channel_TypeDef *ch, const DMA_Config *cfg,
                   volatile void *periph, void *mem, uint16_t count) {
  RCC->AHBENR |= RCC_AHBENR_DMA1EN;

  ch->CCR &= ~DMA_CCR_EN;                     /* CNDTR is only writable off */
  DMA1->IFCR = dma_flag(ch, DMA_IFCR_CGIF1); /* drop the last run's flags */
  ch->CPAR = (uint32_t)(uintptr_t)periph;
  ch->CMAR = (uint32_t)(uintptr_t)mem;
  ch->CNDTR = count;
  ch->CCR = hal_dma_calc_ccr(cfg);
  ch->CCR |= DMA_CCR_EN;
}

void hal_dma_stop(DMA_Channel_TypeDef *ch) { ch->CCR &= ~DMA_CCR_EN; }

uint16_t hal_dma_remaining(const DMA_Channel_TypeDef *ch) {
  return (uint16_t)ch->CNDTR;
}

bool hal_dma_done(const DMA_Channel_TypeDef *ch) {
  return (DMA1->ISR & dma_flag(ch, DMA_ISR_TCIF1)) != 0U;
}

bool hal_dma_error(const DMA_Channel_TypeDef *ch) {
  return (DMA1->ISR & dma_flag(ch, DMA_ISR_TEIF1)) != 0U;
}
