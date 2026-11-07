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

typedef enum {
  CAN_MODE_NORMAL,
  CAN_MODE_LOOPBACK,        /* TX fed back to RX inside the chip, no ACK needed */
  CAN_MODE_SILENT,          /* listen only, never drives the bus */
  CAN_MODE_SILENT_LOOPBACK, /* self-test without touching the bus */
} CAN_Mode;

typedef struct {
  uint32_t bitrate;
  uint16_t sample_permille; /* 0 picks 875 (87.5%) */
  CAN_Mode mode;
  uint32_t timeout_ms; /* for the init-mode handshakes */
} CAN_Config;

typedef struct {
  uint8_t tec;        /* transmit error counter */
  uint8_t rec;        /* receive error counter */
  uint8_t last_error; /* LEC: 0 none, 1 stuff, 2 form, 3 ack, 4/5 bit, 6 crc */
  bool warning;       /* a counter reached 96 */
  bool passive;       /* a counter passed 127 */
  bool bus_off;       /* tec passed 255, node off the bus */
  bool rx_overrun;    /* a frame was lost to a full FIFO since the last call */
} CAN_Errors;

/* Pins are the caller's: TX as AF push-pull, RX as input. For PB8/PB9 call
 * hal_afio_remap(AFIO_MAPR_CAN_REMAP, AFIO_MAPR_CAN_REMAP_REMAP2) first.
 * Needs hal_systick_init() for the timeouts. Leaves bank 0 accepting all. */
HAL_Status hal_can_init(CAN_TypeDef *can, const CAN_Config *cfg);
void hal_can_deinit(CAN_TypeDef *can);

/* bank 0: every frame to FIFO 0 */
void hal_can_filter_accept_all(CAN_TypeDef *can);
/* accept when (frame_id & mask) == (id & mask), same id type only */
void hal_can_filter_mask(CAN_TypeDef *can, uint8_t bank, uint32_t id,
                         uint32_t mask, bool extended, uint8_t fifo);

/* HAL_OK once queued, HAL_ERR_BUSY while all 3 mailboxes are pending */
HAL_Status hal_can_try_send(CAN_TypeDef *can, const CAN_Frame *f);
/* HAL_OK with a frame, HAL_ERR_EMPTY if that FIFO (0 or 1) holds none */
HAL_Status hal_can_try_receive(CAN_TypeDef *can, uint8_t fifo, CAN_Frame *f);
void hal_can_get_errors(CAN_TypeDef *can, CAN_Errors *e);

HAL_Status hal_can_calc_timing(uint32_t pclk_hz, uint32_t bitrate,
                               uint16_t sample_permille, CAN_Timing *t);

uint32_t hal_can_calc_btr(const CAN_Timing *t);

void hal_can_frame_to_mailbox(const CAN_Frame *f, CAN_TxMailBox_TypeDef *mb);

void hal_can_mailbox_to_frame(const CAN_FIFOMailBox_TypeDef *mb, CAN_Frame *f);

uint32_t hal_can_calc_filter_id(uint32_t id, bool extended);
uint32_t hal_can_calc_filter_mask(uint32_t mask, bool extended);

#endif /* HAL_CAN_H */
