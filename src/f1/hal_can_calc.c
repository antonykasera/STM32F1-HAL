/**
 * @file hal_can_calc.c
 * @brief CAN bit-timing arithmetic and mailbox packing
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_TQ_MIN 8U  /* practical minimum tq per bit */
#define CAN_TQ_MAX 25U /* 1 sync + 16 ts1 + 8 ts2 */
#define CAN_BRP_MAX 1024U
#define CAN_TS1_MAX 16U
#define CAN_TS2_MAX 8U
#define CAN_SJW_MAX 4U
#define CAN_STD_ID_MASK 0x7FFU
#define CAN_EXT_ID_MASK 0x1FFFFFFFU
#define CAN_DLC_MAX 8U

/* split n tq into sync + ts1 + ts2 with the sample point nearest sp_permille */
static void can_split(uint32_t n, uint32_t sp_permille, uint32_t *ts1,
                      uint32_t *ts2) {
  uint32_t t1 = ((n * sp_permille) + 500U) / 1000U; /* 1 + ts1, rounded */
  t1 = (t1 > 1U) ? (t1 - 1U) : 1U;
  if (t1 > CAN_TS1_MAX) {
    t1 = CAN_TS1_MAX;
  }
  if (t1 > (n - 2U)) {
    t1 = n - 2U; /* keep ts2 >= 1 */
  }

  uint32_t t2 = n - 1U - t1;
  if (t2 > CAN_TS2_MAX) {
    t2 = CAN_TS2_MAX;
    t1 = n - 1U - t2;
  }

  *ts1 = t1;
  *ts2 = t2;
}

HAL_Status hal_can_calc_timing(uint32_t pclk_hz, uint32_t bitrate,
                               uint16_t sample_permille, CAN_Timing *t) {
  if ((bitrate == 0U) || (sample_permille == 0U) ||
      (sample_permille >= 1000U) || (t == NULL)) {
    return HAL_ERR_PARAM;
  }

  bool found = false;
  uint32_t best_err = 0U;
  uint32_t best_n = 1U;
  CAN_Timing best = {0};

  /* most tq first: on an equal sample-point error, finer timing wins */
  for (uint32_t n = CAN_TQ_MAX; n >= CAN_TQ_MIN; n--) {
    uint64_t cycles_per_bit = (uint64_t)bitrate * n;
    if ((pclk_hz % cycles_per_bit) != 0U) {
      continue; /* bitrate must be exact */
    }
    uint64_t brp = pclk_hz / cycles_per_bit;
    if ((brp == 0U) || (brp > CAN_BRP_MAX)) {
      continue;
    }

    uint32_t ts1 = 0U;
    uint32_t ts2 = 0U;
    can_split(n, sample_permille, &ts1, &ts2);

    /* |sample point - target|, scaled by n to stay in integers */
    uint32_t got = (1U + ts1) * 1000U;
    uint32_t want = (uint32_t)sample_permille * n;
    uint32_t err = (got > want) ? (got - want) : (want - got);

    if (!found || (((uint64_t)err * best_n) < ((uint64_t)best_err * n))) {
      found = true;
      best_err = err;
      best_n = n;
      best.brp = (uint16_t)brp;
      best.ts1 = (uint8_t)ts1;
      best.ts2 = (uint8_t)ts2;
      best.sjw = (uint8_t)((ts2 < CAN_SJW_MAX) ? ts2 : CAN_SJW_MAX);
    }
  }

  if (!found) {
    return HAL_ERR_PARAM;
  }
  *t = best;
  return HAL_OK;
}

uint32_t hal_can_calc_btr(const CAN_Timing *t) {
  return ((uint32_t)(t->sjw - 1U) << CAN_BTR_SJW_Pos) |
         ((uint32_t)(t->ts2 - 1U) << CAN_BTR_TS2_Pos) |
         ((uint32_t)(t->ts1 - 1U) << CAN_BTR_TS1_Pos) |
         ((uint32_t)(t->brp - 1U) << CAN_BTR_BRP_Pos);
}

static uint32_t can_pack4(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}

static void can_unpack4(uint32_t w, uint8_t *b) {
  for (uint32_t i = 0U; i < 4U; i++) {
    b[i] = (uint8_t)(w >> (8U * i));
  }
}

/* TXRQ (bit 0) stays clear: the driver sets it once the mailbox is filled */
void hal_can_frame_to_mailbox(const CAN_Frame *f, CAN_TxMailBox_TypeDef *mb) {
  uint32_t tir;
  if (f->extended) {
    tir = ((f->id & CAN_EXT_ID_MASK) << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
  } else {
    tir = (f->id & CAN_STD_ID_MASK) << CAN_TI0R_STID_Pos;
  }
  if (f->remote) {
    tir |= CAN_TI0R_RTR;
  }

  mb->TIR = tir;
  mb->TDTR = (f->dlc > CAN_DLC_MAX) ? CAN_DLC_MAX : f->dlc;
  mb->TDLR = can_pack4(&f->data[0]); /* data[0] is sent first */
  mb->TDHR = can_pack4(&f->data[4]);
}

void hal_can_mailbox_to_frame(const CAN_FIFOMailBox_TypeDef *mb, CAN_Frame *f) {
  uint32_t rir = mb->RIR;

  f->extended = (rir & CAN_RI0R_IDE) != 0U;
  f->remote = (rir & CAN_RI0R_RTR) != 0U;
  if (f->extended) {
    f->id = (rir >> CAN_RI0R_EXID_Pos) & CAN_EXT_ID_MASK;
  } else {
    f->id = (rir >> CAN_RI0R_STID_Pos) & CAN_STD_ID_MASK;
  }

  /* RDTR also holds the filter index and timestamp; DLC 9..15 means 8 */
  uint8_t dlc = (uint8_t)(mb->RDTR & CAN_RDT0R_DLC_Msk);
  f->dlc = (dlc > CAN_DLC_MAX) ? (uint8_t)CAN_DLC_MAX : dlc;

  can_unpack4(mb->RDLR, &f->data[0]);
  can_unpack4(mb->RDHR, &f->data[4]);
}

/* filter registers use the same layout as RIR */
uint32_t hal_can_calc_filter_id(uint32_t id, bool extended) {
  if (extended) {
    return ((id & CAN_EXT_ID_MASK) << CAN_RI0R_EXID_Pos) | CAN_RI0R_IDE;
  }
  return (id & CAN_STD_ID_MASK) << CAN_RI0R_STID_Pos;
}

/* IDE is always compared, so a filter never matches the other id type */
uint32_t hal_can_calc_filter_mask(uint32_t mask, bool extended) {
  return hal_can_calc_filter_id(mask, extended) | CAN_RI0R_IDE;
}
