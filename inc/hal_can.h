/**
 * @file hal_can.h
 * @brief bxCAN (CAN 2.0A/B) API for the single CAN1 on the F103
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_CAN_H
#define HAL_CAN_H

#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

/* bit timing in time quanta (tq); BTR stores each field minus one */
typedef struct {
  uint16_t brp; /* pclk1 cycles per tq, 1..1024 */
  uint8_t ts1;  /* 1..16 tq; sample point = (1 + ts1) / (1 + ts1 + ts2) */
  uint8_t ts2;  /* 1..8 tq */
  uint8_t sjw;  /* 1..4 tq, resync jump width */
} CAN_Timing;

typedef struct {
  uint32_t id;   /* 11-bit standard or 29-bit extended */
  bool extended; /* IDE */
  bool remote;   /* RTR: requests data, carries none */
  uint8_t dlc;   /* 0..8 */
  uint8_t data[8];
} CAN_Frame;

HAL_Status hal_can_calc_timing(uint32_t pclk_hz, uint32_t bitrate,
                               uint16_t sample_permille, CAN_Timing *t);

uint32_t hal_can_calc_btr(const CAN_Timing *t);

void hal_can_frame_to_mailbox(const CAN_Frame *f, CAN_TxMailBox_TypeDef *mb);

void hal_can_mailbox_to_frame(const CAN_FIFOMailBox_TypeDef *mb, CAN_Frame *f);

#endif /* HAL_CAN_H */
