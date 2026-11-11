/**
 * @file test_usb.c
 * @brief Host-side unit tests for the USB register math, rings and requests
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * The EPnR tests run hal_usb_calc_epr's output through a model of what the
 * hardware does with a write: rw bits take the value, toggle bits flip on 1,
 * CTR bits clear on 0, SETUP ignores writes.
 */

#include "hal_usb.h"
#include "stm32f103xb.h"
#include "test_assert.h"
#include <string.h>

#define RW (USB_EP_T_FIELD | USB_EP_KIND | USB_EPADDR_FIELD)
#define TOGGLE (USB_EP_DTOG_RX | USB_EPRX_STAT | USB_EP_DTOG_TX | USB_EPTX_STAT)
#define CTR (USB_EP_CTR_RX | USB_EP_CTR_TX)

static uint16_t hw_write(uint16_t cur, uint16_t w) {
  return (uint16_t)((w & RW) | ((cur ^ w) & TOGGLE) | (cur & w & CTR) |
                    (cur & USB_EP_SETUP));
}

static uint16_t apply(uint16_t cur, uint16_t mask, uint16_t want) {
  return hw_write(cur, hal_usb_calc_epr(cur, mask, want));
}

void test_usb_epr_cases(void) {
  /* control EP mid-transfer: both toggles 1, CTR_RX pending, TX NAK */
  uint16_t cur = USB_EP_CONTROL | USB_EP_DTOG_RX | USB_EP_DTOG_TX |
                 USB_EP_CTR_RX | USB_EP_TX_NAK | USB_EP_RX_NAK;

  ASSERT_EQ(apply(cur, USB_EPTX_STAT, USB_EP_TX_VALID),
            (cur & ~USB_EPTX_STAT) | USB_EP_TX_VALID);
  ASSERT_EQ(apply(cur, USB_EP_CTR_RX, 0U), cur & ~USB_EP_CTR_RX);
  /* a CTR the hardware raises between our read and write must survive, so
   * CTR bits not being cleared are always written as 1 */
  ASSERT_EQ(hal_usb_calc_epr(0U, USB_EPTX_STAT, USB_EP_TX_VALID) & CTR, CTR);
  ASSERT_EQ(hal_usb_calc_epr(0U, USB_EP_CTR_RX, 0U) & CTR, USB_EP_CTR_TX);
  /* from any state to a clean, freshly configured endpoint */
  ASSERT_EQ(apply(0xFFFFU, 0xFFFFU, USB_EP_BULK | 1U | USB_EP_RX_VALID),
            USB_EP_BULK | 1U | USB_EP_RX_VALID | USB_EP_SETUP);
}

void test_usb_epr_exhaustive(void) {
  static const uint16_t masks[5] = {USB_EPTX_STAT, USB_EPRX_STAT,
                                    USB_EP_CTR_RX, USB_EP_CTR_TX,
                                    USB_EP_DTOG_TX | USB_EPTX_STAT};
  static const uint16_t wants[5] = {USB_EP_TX_STALL, USB_EP_RX_VALID, 0U, 0U,
                                    USB_EP_TX_NAK};
  unsigned bad = 0U;

  for (uint32_t c = 0U; c <= 0xFFFFU; c++) {
    uint16_t cur = (uint16_t)c;
    for (uint32_t k = 0U; k < 5U; k++) {
      uint16_t got = apply(cur, masks[k], wants[k]);
      uint16_t expect = (uint16_t)((cur & ~masks[k]) | wants[k]);
      bad += (got != expect) ? 1U : 0U;
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_usb_count_rx(void) {
  ASSERT_EQ(hal_usb_calc_count_rx(64U), 0x8400U); /* BL_SIZE 1, 2 x 32 */
  ASSERT_EQ(hal_usb_calc_count_rx(8U), 0x1000U);  /* 4 x 2 */
  ASSERT_EQ(hal_usb_calc_count_rx(7U), 0x1000U);  /* rounds up */
  ASSERT_EQ(hal_usb_calc_count_rx(62U), 0x7C00U); /* largest with 2 B blocks */
  ASSERT_EQ(hal_usb_calc_count_rx(96U), 0x8800U);
  ASSERT_EQ(hal_usb_calc_count_rx(512U), 0xBC00U);
}

void test_usb_ring_wraps(void) {
  uint8_t mem[8];
  USB_Ring r = {mem, 8U, 65533U, 65533U}; /* indices about to wrap */
  const uint8_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t out[8] = {0};

  ASSERT_EQ(hal_usb_ring_write(&r, in, 5U), 5U);
  ASSERT_EQ(hal_usb_ring_read(&r, out, 3U), 3U);
  ASSERT_EQ(out[2], 3U);
  ASSERT_EQ(hal_usb_ring_write(&r, &in[5], 3U), 3U);
  ASSERT_EQ(hal_usb_ring_write(&r, in, 8U), 3U); /* only 3 free */
  ASSERT_EQ(hal_usb_ring_free(&r), 0U);
  ASSERT_EQ(hal_usb_ring_read(&r, out, 8U), 8U);
  ASSERT_EQ(out[0], 4U); /* 4..8 then 1..3, in order */
  ASSERT_EQ(out[4], 8U);
  ASSERT_EQ(out[7], 3U);
  ASSERT_EQ(hal_usb_ring_read(&r, out, 8U), 0U);
}

/* every split of packets up to 9 bytes, both directions, odd and even spans */
void test_usb_pma_spans(void) {
  uint8_t src[9], a[10], b[10];
  for (uint8_t i = 0U; i < 9U; i++) {
    src[i] = (uint8_t)(0xA0U + i);
  }
  for (uint16_t n = 0U; n <= 9U; n++) {
    for (uint16_t na = 0U; na <= n; na++) {
      uint16_t pma[20];
      memset(pma, 0xEE, sizeof pma);
      hal_usb_calc_pma_write(pma, src, na, &src[na], (uint16_t)(n - na));
      for (uint16_t k = 0U; k < 10U; k++) {
        uint16_t want = 0xEEEEU; /* odd slots: never touched */
        if (2U * k < n) {
          want = src[2U * k];
          if (2U * k + 1U < n) {
            want = (uint16_t)(want | (src[2U * k + 1U] << 8));
          }
        }
        ASSERT_EQ(pma[2U * k], want);
        ASSERT_EQ(pma[2U * k + 1U], 0xEEEEU);
      }
      memset(a, 0, sizeof a);
      memset(b, 0, sizeof b);
      hal_usb_calc_pma_read(pma, a, na, b, (uint16_t)(n - na));
      ASSERT_TRUE(memcmp(a, src, na) == 0);
      ASSERT_TRUE(memcmp(b, &src[na], n - na) == 0);
      ASSERT_EQ(b[n - na], 0U); /* nothing past the packet */
    }
  }
}

/* random writes and reads of every length against a FIFO model */
void test_usb_ring_random(void) {
  uint8_t mem[64], model[64], buf[84];
  USB_Ring r = {mem, 64U, 65500U, 65500U};
  uint32_t seed = 12345U, in = 0U, out = 0U;
  for (int step = 0; step < 20000; step++) {
    seed = seed * 1103515245U + 12345U;
    uint16_t n = (uint16_t)((seed >> 16) % 70U);
    uint8_t *tmp = buf + 1U + ((seed >> 8) & 3U); /* any alignment */
    memset(buf, 0xEE, sizeof buf);
    if ((seed & 0x8000U) != 0U) {
      for (uint16_t i = 0U; i < n; i++) {
        tmp[i] = (uint8_t)(in + i);
      }
      size_t w = hal_usb_ring_write(&r, tmp, n);
      ASSERT_EQ(w, (n < 64U - (in - out)) ? n : 64U - (in - out));
      for (size_t i = 0U; i < w; i++) {
        model[(in + i) % 64U] = (uint8_t)(in + i);
      }
      in += (uint32_t)w;
    } else {
      size_t got = hal_usb_ring_read(&r, tmp, n);
      ASSERT_EQ(got, (n < in - out) ? n : in - out);
      for (size_t i = 0U; i < got; i++) {
        ASSERT_EQ(tmp[i], model[(out + i) % 64U]);
      }
      ASSERT_EQ(tmp[-1], 0xEE); /* nothing written outside */
      ASSERT_EQ(tmp[got], 0xEE);
      out += (uint32_t)got;
    }
  }
}

static USB_Device make_dev(void) {
  USB_Device d;
  const uint32_t uid[3] = {0x12345678U, 0x9ABCDEF0U, 0x0F1E2D3CU};
  memset(&d, 0xA5, sizeof d);
  hal_usb_calc_device_init(&d, uid);
  return d;
}

static USB_Reply req(USB_Device *d, uint8_t type, uint8_t request,
                     uint16_t value, uint16_t index, uint16_t length) {
  USB_Setup s = {type, request, value, index, length};
  return hal_usb_calc_setup(d, &s);
}

void test_usb_parse_setup(void) {
  const uint8_t raw[8] = {0x80, 0x06, 0x00, 0x01, 0x09, 0x04, 0x40, 0x00};
  USB_Setup s = hal_usb_calc_parse_setup(raw);

  ASSERT_EQ(s.bmRequestType, 0x80U);
  ASSERT_EQ(s.bRequest, 6U);
  ASSERT_EQ(s.wValue, 0x0100U);
  ASSERT_EQ(s.wIndex, 0x0409U);
  ASSERT_EQ(s.wLength, 64U);
}

void test_usb_device_descriptor(void) {
  USB_Device d = make_dev();
  USB_Reply r = req(&d, 0x80, 6, 0x0100, 0, 64);

  ASSERT_EQ(r.kind, USB_REPLY_IN);
  ASSERT_EQ(r.len, 18U);
  ASSERT_TRUE(!r.zlp);
  ASSERT_EQ(r.data[7], 64U); /* EP0 max packet */
  ASSERT_EQ(r.data[8] | (r.data[9] << 8), USB_VID);
  ASSERT_EQ(r.data[10] | (r.data[11] << 8), USB_PID);

  ASSERT_EQ(req(&d, 0x80, 6, 0x0100, 0, 8).len, 8U); /* macOS asks for 8 */
  ASSERT_EQ(req(&d, 0x80, 6, 0x0600, 0, 10).kind, USB_REPLY_STALL); /* FS only */
}

void test_usb_config_descriptor(void) {
  USB_Device d = make_dev();
  USB_Reply r = req(&d, 0x80, 6, 0x0200, 0, 255);
  uint32_t walked = 0U;
  unsigned interfaces = 0U;
  unsigned endpoints = 0U;

  ASSERT_EQ(r.len, 67U);
  ASSERT_EQ(r.data[2] | (r.data[3] << 8), 67U); /* wTotalLength */
  while (walked < r.len) {                      /* sub-descriptors tile it */
    ASSERT_TRUE(r.data[walked] != 0U);
    interfaces += (r.data[walked + 1U] == 4U) ? 1U : 0U;
    endpoints += (r.data[walked + 1U] == 5U) ? 1U : 0U;
    walked += r.data[walked];
  }
  ASSERT_EQ(walked, 67U);
  ASSERT_EQ(interfaces, 2U);
  ASSERT_EQ(endpoints, 3U);
  ASSERT_EQ(req(&d, 0x80, 6, 0x0200, 0, 9).len, 9U); /* header first */
}

void test_usb_string_descriptors(void) {
  USB_Device d = make_dev();
  USB_Reply r = req(&d, 0x80, 6, 0x0300, 0, 255);

  ASSERT_EQ(r.len, 4U);
  ASSERT_EQ(r.data[2], 0x09U); /* 0x0409 en-US */

  r = req(&d, 0x80, 6, 0x0302, 0x0409, 255);
  ASSERT_EQ(r.len, 2U + (2U * (sizeof(USB_PRODUCT) - 1U)));
  ASSERT_EQ(r.data[1], 3U);
  ASSERT_EQ(r.data[2], (uint8_t)USB_PRODUCT[0]);
  ASSERT_EQ(r.data[3], 0U);

  r = req(&d, 0x80, 6, 0x0303, 0x0409, 255);
  ASSERT_EQ(r.len, 50U);
  ASSERT_TRUE(strcmp(d.serial, "123456789ABCDEF00F1E2D3C") == 0);
  ASSERT_EQ(r.data[2], '1');
  ASSERT_EQ(r.data[48], 'C');

  ASSERT_EQ(req(&d, 0x80, 6, 0x0304, 0x0409, 255).kind, USB_REPLY_STALL);
}

void test_usb_address_and_config(void) {
  USB_Device d = make_dev();
  USB_Reply r = req(&d, 0x00, 5, 7, 0, 0);

  ASSERT_EQ(r.kind, USB_REPLY_STATUS);
  ASSERT_EQ(r.action, USB_ACTION_SET_ADDRESS);
  ASSERT_EQ(d.address, 7U);

  r = req(&d, 0x00, 9, 1, 0, 0);
  ASSERT_EQ(r.action, USB_ACTION_SET_CONFIG);
  ASSERT_EQ(d.config, 1U);
  r = req(&d, 0x80, 8, 0, 0, 1);
  ASSERT_EQ(r.len, 1U);
  ASSERT_EQ(r.data[0], 1U);
  ASSERT_EQ(req(&d, 0x00, 9, 2, 0, 0).kind, USB_REPLY_STALL);

  r = req(&d, 0x02, 1, 0, 0x81, 0); /* CLEAR_FEATURE(HALT) on 0x81 */
  ASSERT_EQ(r.action, USB_ACTION_CLEAR_HALT);
  ASSERT_EQ(r.ep, 0x81U);
  ASSERT_EQ(req(&d, 0x02, 1, 0, 0x05, 0).kind, USB_REPLY_STALL);
  ASSERT_EQ(req(&d, 0x40, 1, 0, 0, 0).kind, USB_REPLY_STALL); /* vendor */
}

void test_usb_cdc_requests(void) {
  USB_Device d = make_dev();
  const uint8_t coding[7] = {0x80, 0x25, 0x00, 0x00, 0, 2, 7}; /* 9600 7E1 */

  ASSERT_EQ(d.line_coding[0] | (d.line_coding[1] << 8) |
                (d.line_coding[2] << 16),
            115200U); /* default before the host sets one */

  USB_Setup set = {0x21, 0x20, 0, 0, 7};
  USB_Reply r = hal_usb_calc_setup(&d, &set);
  ASSERT_EQ(r.kind, USB_REPLY_OUT);
  ASSERT_EQ(r.len, 7U);
  hal_usb_calc_setup_out(&d, &set, coding, 7U);
  ASSERT_TRUE(memcmp(d.line_coding, coding, 7U) == 0);

  r = req(&d, 0xA1, 0x21, 0, 0, 7);
  ASSERT_EQ(r.kind, USB_REPLY_IN);
  ASSERT_TRUE(memcmp(r.data, coding, 7U) == 0);

  r = req(&d, 0x21, 0x22, 0x0003, 0, 0); /* DTR + RTS */
  ASSERT_EQ(r.kind, USB_REPLY_STATUS);
  ASSERT_EQ(d.line_state, 3U);
}
