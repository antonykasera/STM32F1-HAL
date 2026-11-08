/**
 * @file test_dma.c
 * @brief Host-side unit tests for the F1 DMA register arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * CCR bits (RM0008): DIR 4, CIRC 5, PINC 6, MINC 7, PSIZE 9:8, MSIZE 11:10,
 * PL 13:12, MEM2MEM 14.
 */

#include "hal_dma.h"
#include "test_assert.h"

void test_dma_ccr_uart_tx(void) {
  /* bytes from an incrementing buffer into a fixed DR */
  DMA_Config c = {DMA_DIR_MEM_TO_PERIPH, DMA_SIZE_8, DMA_SIZE_8,
                  false, true, false, DMA_PRIO_LOW};
  ASSERT_EQ(hal_dma_calc_ccr(&c), 0x0090U); /* DIR | MINC */
}

void test_dma_ccr_adc_scan(void) {
  /* 16-bit results into a circular buffer, high priority */
  DMA_Config c = {DMA_DIR_PERIPH_TO_MEM, DMA_SIZE_16, DMA_SIZE_16,
                  false, true, true, DMA_PRIO_HIGH};
  ASSERT_EQ(hal_dma_calc_ccr(&c), 0x25A0U); /* PL 2, MSIZE/PSIZE 1, MINC, CIRC */
}

void test_dma_ccr_mem_to_mem(void) {
  DMA_Config c = {DMA_DIR_MEM_TO_MEM, DMA_SIZE_32, DMA_SIZE_32,
                  true, true, false, DMA_PRIO_VERY_HIGH};
  ASSERT_EQ(hal_dma_calc_ccr(&c), 0x7AC0U); /* DIR stays 0 for mem-to-mem */
}

void test_dma_channel_index(void) {
  /* address arithmetic only: the pointers are never dereferenced */
  ASSERT_EQ(hal_dma_channel_index(DMA1_Channel1), 0U);
  ASSERT_EQ(hal_dma_channel_index(DMA1_Channel4), 3U);
  ASSERT_EQ(hal_dma_channel_index(DMA1_Channel7), 6U);
}
