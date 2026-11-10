/**
 * @file hal_usb_calc.c
 * @brief USB endpoint register math, packet buffer sizes, byte rings
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * EPnR mixes three bit kinds (RM0008 23.5.2): rw fields, toggle fields (a 1
 * flips the bit) and rc_w0 CTR flags (a 0 clears, a 1 keeps). A plain
 * read-modify-write flips toggles and clears flags by accident.
 */

#include "hal_error.h"
#include "hal_usb.h"
#include "stm32f103xb.h"
#include <stddef.h>
#include <stdint.h>

#define EPR_RW (USB_EP_T_FIELD | USB_EP_KIND | USB_EPADDR_FIELD)
#define EPR_TOGGLE                                                             \
  (USB_EP_DTOG_RX | USB_EPRX_STAT | USB_EP_DTOG_TX | USB_EPTX_STAT)
#define EPR_CTR (USB_EP_CTR_RX | USB_EP_CTR_TX)

#define USB_BARRIER() __asm__ volatile("" ::: "memory")

uint16_t hal_usb_calc_epr(uint16_t epr, uint16_t mask, uint16_t want) {
  uint32_t w = ((epr & ~(uint32_t)mask) | (want & (uint32_t)mask)) & EPR_RW;
  w |= ((uint32_t)epr ^ want) & mask & EPR_TOGGLE;
  w |= (~(uint32_t)mask | want) & EPR_CTR;
  return (uint16_t)w;
}

/* packet memory holds a halfword per 32-bit word: byte pair k at pma[2 k] */
void hal_usb_calc_pma_write(volatile uint16_t *pma, const uint8_t *a,
                            uint16_t na, const uint8_t *b, uint16_t nb) {
  for (; na >= 2U; na = (uint16_t)(na - 2U), a += 2, pma += 2) {
    *pma = (uint16_t)(a[0] | (a[1] << 8));
  }
  if (na != 0U) { /* a halfword across the two spans */
    uint16_t v = a[0];
    if (nb != 0U) {
      v = (uint16_t)(v | (b[0] << 8));
      b++;
      nb--;
    }
    *pma = v;
    pma += 2;
  }
  for (; nb >= 2U; nb = (uint16_t)(nb - 2U), b += 2, pma += 2) {
    *pma = (uint16_t)(b[0] | (b[1] << 8));
  }
  if (nb != 0U) {
    *pma = b[0];
  }
}

void hal_usb_calc_pma_read(const volatile uint16_t *pma, uint8_t *a,
                           uint16_t na, uint8_t *b, uint16_t nb) {
  for (; na >= 2U; na = (uint16_t)(na - 2U), a += 2, pma += 2) {
    uint16_t v = *pma;
    a[0] = (uint8_t)v;
    a[1] = (uint8_t)(v >> 8);
  }
  if (na != 0U) {
    uint16_t v = *pma;
    pma += 2;
    a[0] = (uint8_t)v;
    if (nb != 0U) {
      b[0] = (uint8_t)(v >> 8);
      b++;
      nb--;
    }
  }
  for (; nb >= 2U; nb = (uint16_t)(nb - 2U), b += 2, pma += 2) {
    uint16_t v = *pma;
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
  }
  if (nb != 0U) {
    b[0] = (uint8_t)*pma;
  }
}

/* BL_SIZE 0: 2-byte blocks up to 62 B; BL_SIZE 1: 32-byte blocks */
uint16_t hal_usb_calc_count_rx(uint16_t bytes) {
  HAL_ASSERT((bytes >= 2U) && (bytes <= 512U));

  if (bytes <= 62U) {
    return (uint16_t)(((bytes + 1U) / 2U) << 10);
  }
  return (uint16_t)(0x8000U | ((((bytes + 31U) / 32U) - 1U) << 10));
}

size_t hal_usb_ring_count(const USB_Ring *r) {
  return (uint16_t)(r->head - r->tail);
}

size_t hal_usb_ring_free(const USB_Ring *r) {
  return r->size - hal_usb_ring_count(r);
}

static void copy(uint8_t *dst, const uint8_t *src, size_t n) {
  for (size_t i = 0U; i < n; i++) {
    dst[i] = src[i];
  }
}

size_t hal_usb_ring_write(USB_Ring *r, const uint8_t *src, size_t n) {
  uint16_t head = r->head;
  size_t room = hal_usb_ring_free(r);
  if (n > room) {
    n = room;
  }
  size_t at = head & (r->size - 1U);
  size_t first = (n < r->size - at) ? n : r->size - at; /* before the wrap */
  copy(&r->buf[at], src, first);
  copy(r->buf, &src[first], n - first);
  USB_BARRIER(); /* data before the index the other side reads */
  r->head = (uint16_t)(head + n);
  return n;
}

size_t hal_usb_ring_read(USB_Ring *r, uint8_t *dst, size_t n) {
  uint16_t tail = r->tail;
  size_t avail = hal_usb_ring_count(r);
  if (n > avail) {
    n = avail;
  }
  size_t at = tail & (r->size - 1U);
  size_t first = (n < r->size - at) ? n : r->size - at;
  copy(dst, &r->buf[at], first);
  copy(&dst[first], r->buf, n - first);
  USB_BARRIER();
  r->tail = (uint16_t)(tail + n);
  return n;
}
