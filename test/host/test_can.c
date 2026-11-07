/**
 * @file test_can.c
 * @brief Host-side unit tests for the F1 CAN timing math and frame packing
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Timing checks: pclk1 = brp * (1 + ts1 + ts2) * bitrate exactly, with the
 * sample point (1 + ts1) / (1 + ts1 + ts2) closest to the target; ties go to
 * more tq per bit. Values worked by hand for pclk1 = 36 MHz (RM0008 bxCAN).
 */

#include "hal_can.h"
#include "test_assert.h"

#define PCLK1 36000000U

static void check_timing(uint32_t pclk, uint32_t bitrate, uint16_t sp,
                         uint16_t brp, uint8_t ts1, uint8_t ts2, uint8_t sjw) {
  CAN_Timing t = {0};

  ASSERT_EQ(hal_can_calc_timing(pclk, bitrate, sp, &t), HAL_OK);
  ASSERT_EQ(t.brp, brp);
  ASSERT_EQ(t.ts1, ts1);
  ASSERT_EQ(t.ts2, ts2);
  ASSERT_EQ(t.sjw, sjw);
  ASSERT_EQ((uint32_t)t.brp * (1U + t.ts1 + t.ts2) * bitrate, pclk);
}

void test_can_timing_common_rates(void) {
  /* 87.5% is the usual CANopen / DroneCAN sample point */
  check_timing(PCLK1, 125000U, 875U, 18U, 13U, 2U, 2U); /* 16 tq, 87.5% */
  check_timing(PCLK1, 250000U, 875U, 9U, 13U, 2U, 2U);  /* 16 tq, 87.5% */
  check_timing(PCLK1, 500000U, 875U, 9U, 6U, 1U, 1U);   /* 8 tq, 87.5% */
  check_timing(PCLK1, 1000000U, 875U, 2U, 15U, 2U, 2U); /* 18 tq, 88.9% */
}

void test_can_timing_tie_prefers_more_tq(void) {
  /* 75% at 500k: 12 tq (9/12) and 8 tq (6/8) are both exact; 12 wins */
  check_timing(PCLK1, 500000U, 750U, 6U, 8U, 3U, 3U);
}

void test_can_timing_hsi_default_clock(void) {
  /* 8 MHz pclk1 (HSI, no PLL): 16 tq per bit at 500k */
  check_timing(8000000U, 500000U, 875U, 1U, 13U, 2U, 2U);
}

void test_can_timing_rejects_impossible(void) {
  CAN_Timing t = {7U, 7U, 7U, 7U};

  ASSERT_EQ(hal_can_calc_timing(PCLK1, 0U, 875U, &t), HAL_ERR_PARAM);
  /* 36e6 / 700e3 is not a whole number of pclk cycles per bit */
  ASSERT_EQ(hal_can_calc_timing(PCLK1, 700000U, 875U, &t), HAL_ERR_PARAM);
  /* 1 kbit/s needs brp > 1024 even at 25 tq */
  ASSERT_EQ(hal_can_calc_timing(PCLK1, 1000U, 875U, &t), HAL_ERR_PARAM);
  ASSERT_EQ(hal_can_calc_timing(PCLK1, 500000U, 0U, &t), HAL_ERR_PARAM);
  ASSERT_EQ(hal_can_calc_timing(PCLK1, 500000U, 1000U, &t), HAL_ERR_PARAM);
  ASSERT_EQ(t.brp, 7U); /* untouched on error */
}

void test_can_btr_stores_minus_one(void) {
  CAN_Timing t500 = {9U, 6U, 1U, 1U};
  CAN_Timing t1m = {2U, 15U, 2U, 2U};

  ASSERT_EQ(hal_can_calc_btr(&t500), 0x00050008U);
  ASSERT_EQ(hal_can_calc_btr(&t1m), 0x011E0001U);
}

void test_can_frame_to_mailbox_standard(void) {
  CAN_TxMailBox_TypeDef mb = {0};
  CAN_Frame f = {0x123U, false, false, 3U, {0x11U, 0x22U, 0x33U}};

  hal_can_frame_to_mailbox(&f, &mb);

  ASSERT_EQ(mb.TIR, 0x123U << 21); /* STID in 31:21, TXRQ left clear */
  ASSERT_EQ(mb.TDTR, 3U);
  ASSERT_EQ(mb.TDLR, 0x00332211U); /* data[0] in the low byte */
  ASSERT_EQ(mb.TDHR, 0U);
}

void test_can_frame_to_mailbox_extended(void) {
  CAN_TxMailBox_TypeDef mb = {0};
  CAN_Frame f = {0x12345678U, true, false, 8U, {1, 2, 3, 4, 5, 6, 7, 8}};

  hal_can_frame_to_mailbox(&f, &mb);

  ASSERT_EQ(mb.TIR, (0x12345678U << 3) | CAN_TI0R_IDE); /* EXID in 31:3 */
  ASSERT_EQ(mb.TDTR, 8U);
  ASSERT_EQ(mb.TDLR, 0x04030201U);
  ASSERT_EQ(mb.TDHR, 0x08070605U);
}

void test_can_frame_to_mailbox_remote(void) {
  CAN_TxMailBox_TypeDef mb = {0};
  CAN_Frame f = {0x7FFU, false, true, 0U, {0}};

  hal_can_frame_to_mailbox(&f, &mb);

  ASSERT_EQ(mb.TIR, (0x7FFU << 21) | CAN_TI0R_RTR);
  ASSERT_EQ(mb.TDTR, 0U);
}

void test_can_mailbox_to_frame(void) {
  CAN_FIFOMailBox_TypeDef mb = {0};
  CAN_Frame f = {0};

  /* RDTR also carries the filter match index and timestamp above the DLC */
  mb.RIR = 0x123U << 21;
  mb.RDTR = 3U | (5U << 8) | (0xBEEFU << 16);
  mb.RDLR = 0x00332211U;
  hal_can_mailbox_to_frame(&mb, &f);
  ASSERT_EQ(f.id, 0x123U);
  ASSERT_TRUE(!f.extended);
  ASSERT_TRUE(!f.remote);
  ASSERT_EQ(f.dlc, 3U);
  ASSERT_EQ(f.data[0], 0x11U);
  ASSERT_EQ(f.data[2], 0x33U);

  mb.RIR = (0x12345678U << 3) | CAN_RI0R_IDE | CAN_RI0R_RTR;
  mb.RDTR = 0xFU; /* CAN 2.0: DLC 9..15 still means 8 bytes */
  hal_can_mailbox_to_frame(&mb, &f);
  ASSERT_EQ(f.id, 0x12345678U);
  ASSERT_TRUE(f.extended);
  ASSERT_TRUE(f.remote);
  ASSERT_EQ(f.dlc, 8U);
}

void test_can_filter_values(void) {
  /* same layout as RIR; the mask always compares IDE */
  ASSERT_EQ(hal_can_calc_filter_id(0x123U, false), 0x123U << 21);
  ASSERT_EQ(hal_can_calc_filter_mask(0x7FFU, false), 0xFFE00004U);
  ASSERT_EQ(hal_can_calc_filter_id(0x12345678U, true),
            (0x12345678U << 3) | CAN_RI0R_IDE);
  ASSERT_EQ(hal_can_calc_filter_mask(0x1FFFFFFFU, true), 0xFFFFFFFCU);
}

/* driver tests on a CAN_TypeDef in host RAM */

void test_can_filter_mask_bank(void) {
  static CAN_TypeDef c; /* static: zeroed and off the stack */
  c.FMR = CAN_FMR_FINIT;
  c.FM1R = 1U << 3;

  hal_can_filter_mask(&c, 3U, 0x123U, 0x7FFU, false, 1U);

  ASSERT_EQ(c.FS1R, 1U << 3);  /* 32-bit scale */
  ASSERT_EQ(c.FM1R, 0U);       /* mask mode */
  ASSERT_EQ(c.FFA1R, 1U << 3); /* FIFO 1 */
  ASSERT_EQ(c.FA1R, 1U << 3);  /* active */
  ASSERT_EQ(c.FMR & CAN_FMR_FINIT, 0U);
  ASSERT_EQ(c.sFilterRegister[3].FR1, 0x123U << 21);
  ASSERT_EQ(c.sFilterRegister[3].FR2, 0xFFE00004U);
}

void test_can_try_send(void) {
  static CAN_TypeDef c;
  CAN_Frame f = {0x123U, false, false, 1U, {0xAAU}};

  c.TSR = 0U; /* all three mailboxes pending */
  ASSERT_EQ(hal_can_try_send(&c, &f), HAL_ERR_BUSY);

  /* mailboxes 1 and 2 free, CODE names 1 as next */
  c.TSR = CAN_TSR_TME1 | CAN_TSR_TME2 | (1U << CAN_TSR_CODE_Pos);
  ASSERT_EQ(hal_can_try_send(&c, &f), HAL_OK);
  ASSERT_EQ(c.sTxMailBox[1].TIR, (0x123U << 21) | CAN_TI0R_TXRQ);
  ASSERT_EQ(c.sTxMailBox[1].TDLR, 0xAAU);
  ASSERT_EQ(c.sTxMailBox[0].TIR, 0U);
}

void test_can_try_receive(void) {
  static CAN_TypeDef c;
  CAN_Frame f = {0};

  ASSERT_EQ(hal_can_try_receive(&c, 0U, &f), HAL_ERR_EMPTY);

  c.RF1R = 1U; /* FMP1: one frame pending in FIFO 1 */
  c.sFIFOMailBox[1].RIR = 0x321U << 21;
  c.sFIFOMailBox[1].RDTR = 1U;
  c.sFIFOMailBox[1].RDLR = 0x5AU;
  ASSERT_EQ(hal_can_try_receive(&c, 1U, &f), HAL_OK);
  ASSERT_EQ(f.id, 0x321U);
  ASSERT_EQ(f.data[0], 0x5AU);
  ASSERT_EQ(c.RF1R, CAN_RF1R_RFOM1); /* slot released */
}

void test_can_get_errors(void) {
  static CAN_TypeDef c;
  CAN_Errors e = {0};

  c.ESR = (200U << CAN_ESR_REC_Pos) | (130U << CAN_ESR_TEC_Pos) |
          (3U << CAN_ESR_LEC_Pos) | CAN_ESR_EPVF | CAN_ESR_EWGF;
  c.RF0R = CAN_RF0R_FOVR0;

  hal_can_get_errors(&c, &e);

  ASSERT_EQ(e.rec, 200U);
  ASSERT_EQ(e.tec, 130U);
  ASSERT_EQ(e.last_error, 3U); /* ack error: nobody acknowledged */
  ASSERT_TRUE(e.passive);
  ASSERT_TRUE(e.warning);
  ASSERT_TRUE(!e.bus_off);
  ASSERT_TRUE(e.rx_overrun);
  ASSERT_EQ(c.RF1R, 0U); /* FIFO 1 had no overrun: untouched */
}

/* RX queue (filled by the interrupt, drained by the loop) */

static CAN_Frame frame_n(uint32_t n) {
  CAN_Frame f = {0x100U, false, false, 1U, {0}};
  f.data[0] = (uint8_t)n;
  return f;
}

void test_can_queue_fifo_order(void) {
  static CAN_Queue q;
  CAN_Frame out;

  ASSERT_TRUE(!hal_can_queue_pop(&q, &out)); /* empty */
  for (uint32_t i = 0U; i < 5U; i++) {
    CAN_Frame in = frame_n(i);
    ASSERT_TRUE(hal_can_queue_push(&q, &in));
  }
  for (uint32_t i = 0U; i < 5U; i++) {
    ASSERT_TRUE(hal_can_queue_pop(&q, &out));
    ASSERT_EQ(out.data[0], i);
  }
  ASSERT_TRUE(!hal_can_queue_pop(&q, &out));
}

void test_can_queue_full_drops_newest(void) {
  static CAN_Queue q;
  CAN_Frame in;
  CAN_Frame out;

  for (uint32_t i = 0U; i < CAN_RX_QUEUE_LEN; i++) {
    in = frame_n(i);
    ASSERT_TRUE(hal_can_queue_push(&q, &in));
  }
  in = frame_n(99U);
  ASSERT_TRUE(!hal_can_queue_push(&q, &in));
  ASSERT_EQ(q.dropped, 1U);

  ASSERT_TRUE(hal_can_queue_pop(&q, &out));
  ASSERT_EQ(out.data[0], 0U); /* oldest kept, the overflow frame lost */
}

void test_can_queue_index_wrap(void) {
  static CAN_Queue q;
  CAN_Frame out;

  /* indices about to wrap past 2^32: fill level must still be head - tail */
  q.head = 0xFFFFFFFEU;
  q.tail = 0xFFFFFFFEU;
  for (uint32_t i = 0U; i < 4U; i++) {
    CAN_Frame in = frame_n(i);
    ASSERT_TRUE(hal_can_queue_push(&q, &in));
  }
  ASSERT_EQ(q.head, 2U);
  for (uint32_t i = 0U; i < 4U; i++) {
    ASSERT_TRUE(hal_can_queue_pop(&q, &out));
    ASSERT_EQ(out.data[0], i);
  }
  ASSERT_EQ(q.dropped, 0U);
}
