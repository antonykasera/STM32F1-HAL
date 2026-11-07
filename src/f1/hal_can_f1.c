/**
 * @file hal_can_f1.c
 * @brief STM32F1 bxCAN driver: init, filters, polled send/receive
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 */

#include "hal_can.h"
#include "hal_clock.h"
#include "hal_error.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stdint.h>

#define CAN_FILTER_BANKS 14U
#define CAN_SAMPLE_DEFAULT 875U
#define CAN_STD_ID_MAX 0x7FFU
#define CAN_EXT_ID_MAX 0x1FFFFFFFU

static HAL_Status can_wait_msr(const CAN_TypeDef *can, uint32_t mask,
                               uint32_t want, uint32_t timeout_ms) {
  uint32_t start = hal_get_tick();

  while ((can->MSR & mask) != want) {
    if (hal_tick_expired(hal_get_tick(), start, timeout_ms)) {
      return HAL_ERR_TIMEOUT;
    }
  }
  return HAL_OK;
}

static void can_filter_set(CAN_TypeDef *can, uint8_t bank, uint32_t fr1,
                           uint32_t fr2, uint8_t fifo) {
  uint32_t bit = (uint32_t)1U << bank;

  can->FMR |= CAN_FMR_FINIT; /* banks can only change in filter init mode */
  can->FA1R &= ~bit;
  can->FS1R |= bit;  /* one 32-bit id + mask */
  can->FM1R &= ~bit; /* mask mode, not an id list */
  if (fifo == 0U) {
    can->FFA1R &= ~bit;
  } else {
    can->FFA1R |= bit;
  }
  can->sFilterRegister[bank].FR1 = fr1;
  can->sFilterRegister[bank].FR2 = fr2;
  can->FA1R |= bit;
  can->FMR &= ~CAN_FMR_FINIT;
}

HAL_Status hal_can_init(CAN_TypeDef *can, const CAN_Config *cfg) {
  HAL_ASSERT(can == CAN1);
  if (can != CAN1) {
    return HAL_ERR_UNSUPPORTED;
  }

  CAN_Timing t;
  uint16_t sp = (cfg->sample_permille != 0U) ? cfg->sample_permille
                                             : (uint16_t)CAN_SAMPLE_DEFAULT;
  HAL_Status st =
      hal_can_calc_timing(hal_clock_get_pclk1_hz(), cfg->bitrate, sp, &t);
  HAL_ASSERT(st == HAL_OK); /* bitrate not reachable from pclk1 */
  if (st != HAL_OK) {
    return st;
  }

  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

  /* out of sleep (the reset state), into initialization mode */
  can->MCR = (can->MCR & ~CAN_MCR_SLEEP) | CAN_MCR_INRQ;
  st = can_wait_msr(can, CAN_MSR_INAK | CAN_MSR_SLAK, CAN_MSR_INAK,
                    cfg->timeout_ms);
  if (st != HAL_OK) {
    return st;
  }

  /* recover from bus-off by itself; send in request order, not by id */
  can->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;

  uint32_t btr = hal_can_calc_btr(&t);
  if ((cfg->mode == CAN_MODE_LOOPBACK) ||
      (cfg->mode == CAN_MODE_SILENT_LOOPBACK)) {
    btr |= CAN_BTR_LBKM;
  }
  if ((cfg->mode == CAN_MODE_SILENT) ||
      (cfg->mode == CAN_MODE_SILENT_LOOPBACK)) {
    btr |= CAN_BTR_SILM;
  }
  can->BTR = btr;

  hal_can_filter_accept_all(can);

  /* leaving init waits for 11 recessive bits on RX: a timeout here usually
   * means no transceiver or a miswired RX (loopback doesn't use the pin) */
  can->MCR &= ~CAN_MCR_INRQ;
  return can_wait_msr(can, CAN_MSR_INAK, 0U, cfg->timeout_ms);
}

void hal_can_deinit(CAN_TypeDef *can) {
  HAL_ASSERT(can == CAN1);

  can->MCR |= CAN_MCR_RESET; /* master reset, back to sleep */
  RCC->APB1ENR &= ~RCC_APB1ENR_CAN1EN;
}

void hal_can_filter_accept_all(CAN_TypeDef *can) {
  can_filter_set(can, 0U, 0U, 0U, 0U);
}

void hal_can_filter_mask(CAN_TypeDef *can, uint8_t bank, uint32_t id,
                         uint32_t mask, bool extended, uint8_t fifo) {
  HAL_ASSERT(bank < CAN_FILTER_BANKS);
  HAL_ASSERT(fifo < 2U);
  if ((bank >= CAN_FILTER_BANKS) || (fifo >= 2U)) {
    return;
  }

  can_filter_set(can, bank, hal_can_calc_filter_id(id, extended),
                 hal_can_calc_filter_mask(mask, extended), fifo);
}

HAL_Status hal_can_try_send(CAN_TypeDef *can, const CAN_Frame *f) {
  HAL_ASSERT(f->dlc <= 8U);
  HAL_ASSERT(f->id <= (f->extended ? CAN_EXT_ID_MAX : CAN_STD_ID_MAX));

  uint32_t tsr = can->TSR;
  if ((tsr & CAN_TSR_TME) == 0U) {
    return HAL_ERR_BUSY;
  }

  uint32_t box = (tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos; /* next free */
  hal_can_frame_to_mailbox(f, &can->sTxMailBox[box]);
  can->sTxMailBox[box].TIR |= CAN_TI0R_TXRQ;
  return HAL_OK;
}

HAL_Status hal_can_try_receive(CAN_TypeDef *can, uint8_t fifo, CAN_Frame *f) {
  HAL_ASSERT(fifo < 2U);
  if (fifo >= 2U) {
    return HAL_ERR_PARAM;
  }

  volatile uint32_t *rfr = (fifo == 0U) ? &can->RF0R : &can->RF1R;
  if ((*rfr & CAN_RF0R_FMP0) == 0U) {
    return HAL_ERR_EMPTY;
  }

  hal_can_mailbox_to_frame(&can->sFIFOMailBox[fifo], f);
  *rfr = CAN_RF0R_RFOM0; /* release it; FULL/FOVR are rc_w1, 0 leaves them */
  return HAL_OK;
}

void hal_can_get_errors(CAN_TypeDef *can, CAN_Errors *e) {
  uint32_t esr = can->ESR;

  e->tec = (uint8_t)((esr & CAN_ESR_TEC_Msk) >> CAN_ESR_TEC_Pos);
  e->rec = (uint8_t)((esr & CAN_ESR_REC_Msk) >> CAN_ESR_REC_Pos);
  e->last_error = (uint8_t)((esr & CAN_ESR_LEC_Msk) >> CAN_ESR_LEC_Pos);
  e->warning = (esr & CAN_ESR_EWGF) != 0U;
  e->passive = (esr & CAN_ESR_EPVF) != 0U;
  e->bus_off = (esr & CAN_ESR_BOFF) != 0U;

  uint32_t ovr0 = can->RF0R & CAN_RF0R_FOVR0;
  uint32_t ovr1 = can->RF1R & CAN_RF1R_FOVR1;
  e->rx_overrun = (ovr0 | ovr1) != 0U;

  /* rc_w1: write 1 to clear only the overrun flags just read */
  if (ovr0 != 0U) {
    can->RF0R = CAN_RF0R_FOVR0;
  }
  if (ovr1 != 0U) {
    can->RF1R = CAN_RF1R_FOVR1;
  }
}
