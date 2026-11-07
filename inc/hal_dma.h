/**
 * @file hal_dma.h
 * @brief DMA1 channels: polled memory <-> peripheral transfers
 *
 * Each peripheral request is wired to one fixed channel (RM0008 DMA1 request
 * table): ADC1 -> 1, USART3_TX -> 2, USART1_TX -> 4, USART1_RX -> 5,
 * USART2_RX -> 6, USART2_TX -> 7, ...
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_DMA_H
#define HAL_DMA_H

#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  DMA_DIR_PERIPH_TO_MEM,
  DMA_DIR_MEM_TO_PERIPH,
  DMA_DIR_MEM_TO_MEM, /* periph = source, mem = destination */
} DMA_Dir;

typedef enum { DMA_SIZE_8 = 0, DMA_SIZE_16, DMA_SIZE_32 } DMA_Size;

typedef enum {
  DMA_PRIO_LOW = 0,
  DMA_PRIO_MEDIUM,
  DMA_PRIO_HIGH,
  DMA_PRIO_VERY_HIGH,
} DMA_Priority;

typedef struct {
  DMA_Dir dir;
  DMA_Size periph_size;
  DMA_Size mem_size;
  bool periph_inc;
  bool mem_inc;
  bool circular; /* reload count and restart when it reaches 0 */
  DMA_Priority priority;
} DMA_Config;

/* (re)start a channel; count is in transfers of the configured size */
void hal_dma_start(DMA_Channel_TypeDef *ch, const DMA_Config *cfg,
                   volatile void *periph, void *mem, uint16_t count);
void hal_dma_stop(DMA_Channel_TypeDef *ch);

uint16_t hal_dma_remaining(const DMA_Channel_TypeDef *ch);
bool hal_dma_done(const DMA_Channel_TypeDef *ch);  /* TCIF: all transferred */
bool hal_dma_error(const DMA_Channel_TypeDef *ch); /* TEIF: bus error */

uint32_t hal_dma_calc_ccr(const DMA_Config *cfg); /* without EN */
uint32_t hal_dma_channel_index(const DMA_Channel_TypeDef *ch); /* ch1 = 0 */

#endif /* HAL_DMA_H */
