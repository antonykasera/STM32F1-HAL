/**
 * @file hal_usb_calc.c
 * @brief USB endpoint register math, packet buffer sizes, byte rings
 *
 * Copyright (c) 2026.
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

size_t hal_usb_ring_write(USB_Ring *r, const uint8_t *src, size_t n) {
  uint16_t head = r->head;
  size_t room = hal_usb_ring_free(r);
  if (n > room) {
    n = room;
  }
  for (size_t i = 0U; i < n; i++) {
    r->buf[(uint16_t)(head + i) & (r->size - 1U)] = src[i];
  }
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
  for (size_t i = 0U; i < n; i++) {
    dst[i] = r->buf[(uint16_t)(tail + i) & (r->size - 1U)];
  }
  USB_BARRIER();
  r->tail = (uint16_t)(tail + n);
  return n;
}
