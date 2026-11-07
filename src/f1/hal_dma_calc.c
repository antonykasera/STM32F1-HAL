/**
 * @file hal_dma_calc.c
 * @brief DMA channel register arithmetic
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_dma.h"
#include <stdint.h>

#define DMA_CH_STRIDE 0x14U /* CCR, CNDTR, CPAR, CMAR, reserved */

uint32_t hal_dma_calc_ccr(const DMA_Config *cfg) {
  uint32_t ccr = ((uint32_t)cfg->priority << DMA_CCR_PL_Pos) |
                 ((uint32_t)cfg->mem_size << DMA_CCR_MSIZE_Pos) |
                 ((uint32_t)cfg->periph_size << DMA_CCR_PSIZE_Pos);

  if (cfg->mem_inc) {
    ccr |= DMA_CCR_MINC;
  }
  if (cfg->periph_inc) {
    ccr |= DMA_CCR_PINC;
  }
  if (cfg->circular) {
    ccr |= DMA_CCR_CIRC;
  }
  if (cfg->dir == DMA_DIR_MEM_TO_PERIPH) {
    ccr |= DMA_CCR_DIR; /* read from memory */
  } else if (cfg->dir == DMA_DIR_MEM_TO_MEM) {
    ccr |= DMA_CCR_MEM2MEM; /* DIR stays 0: CPAR is the source */
  }
  return ccr;
}

uint32_t hal_dma_channel_index(const DMA_Channel_TypeDef *ch) {
  return ((uint32_t)(uintptr_t)ch - DMA1_Channel1_BASE) / DMA_CH_STRIDE;
}
