/**
 * @file hal_usb.h
 * @brief USB full-speed device as a CDC-ACM virtual serial port
 *
 * Needs SYSCLK = 72 MHz from the PLL (USB clock = PLL / 1.5 = 48 MHz), with
 * the HSE for crystal accuracy, and hal_systick_init(). Shares its 512 B SRAM
 * and IRQ vector with CAN, so link one or the other.
 *
 * Polled: call hal_usb_poll() every few ms (the host NAK-retries while you're
 * busy), or hal_usb_enable_interrupt() to run it from the USB IRQ instead.
 * Linux /dev/ttyACMx, macOS /dev/cu.usbmodem*, Windows 10+ COMx (usbser).
 * The serial number is the chip UID; some F103 clones read it as all zeros.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_USB_H
#define HAL_USB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef USB_VID
#define USB_VID 0x1209U /* pid.codes */
#endif
#ifndef USB_PID
#define USB_PID 0x0001U /* pid.codes test PID: private use only */
#endif

#ifndef USB_MANUFACTURER
#define USB_MANUFACTURER "flex" /* strings: up to 31 chars */
#endif
#ifndef USB_PRODUCT
#define USB_PRODUCT "flex CDC"
#endif

#ifndef USB_CDC_RX_SIZE
#define USB_CDC_RX_SIZE 256U /* power of two, >= 64 */
#endif
#ifndef USB_CDC_TX_SIZE
#define USB_CDC_TX_SIZE 256U /* power of two */
#endif

void hal_usb_init(void);
void hal_usb_poll(void);
void hal_usb_enable_interrupt(uint8_t priority);

bool hal_usb_configured(void);   /* enumerated by the host */
bool hal_usb_cdc_dtr(void);      /* a terminal has the port open */
uint32_t hal_usb_cdc_baud(void); /* as set by the host; USB ignores it */

/* non-blocking: return how many bytes were queued / copied out. Queued TX
 * waits in the device until terminal opens the port. buf: RAM or flash, any
 * alignment; with UNALIGN_TRP set, build with -mno-unaligned-access. */
size_t hal_usb_cdc_write(const void *buf, size_t len);
size_t hal_usb_cdc_read(void *buf, size_t max);

/* ---- internals, exposed for host tests ---- */

#define USB_EP0_SIZE 64U
#define USB_CDC_DATA_SIZE 64U
#define USB_CDC_NOTIFY_SIZE 8U

/* value to write to an EPnR so the fields in mask become want; all others
 * unchanged. CTR bits can only be cleared (want 0). */
uint16_t hal_usb_calc_epr(uint16_t epr, uint16_t mask, uint16_t want);
uint16_t hal_usb_calc_count_rx(uint16_t bytes);

/* a packet to / from packet memory as two spans (a ring's wrap); pma steps
 * by 2 per halfword, the F1's 32-bit stride */
void hal_usb_calc_pma_write(volatile uint16_t *pma, const uint8_t *a,
                            uint16_t na, const uint8_t *b, uint16_t nb);
void hal_usb_calc_pma_read(const volatile uint16_t *pma, uint8_t *a,
                           uint16_t na, uint8_t *b, uint16_t nb);

/* single producer, single consumer; free-running indices */
typedef struct {
  uint8_t *buf;
  uint16_t size;          /* power of two */
  volatile uint16_t head; /* producer only */
  volatile uint16_t tail; /* consumer only */
} USB_Ring;

size_t hal_usb_ring_write(USB_Ring *r, const uint8_t *src, size_t n);
size_t hal_usb_ring_read(USB_Ring *r, uint8_t *dst, size_t n);
size_t hal_usb_ring_count(const USB_Ring *r);
size_t hal_usb_ring_free(const USB_Ring *r);

typedef struct {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} USB_Setup;

typedef enum {
  USB_REPLY_STALL,  /* unsupported: stall both directions */
  USB_REPLY_IN,     /* send data, then expect a status OUT */
  USB_REPLY_OUT,    /* receive len bytes, then send a status IN */
  USB_REPLY_STATUS, /* no data stage: send a zero-length status IN */
} USB_ReplyKind;

typedef enum {
  USB_ACTION_NONE,
  USB_ACTION_SET_ADDRESS, /* only after the status IN has gone out */
  USB_ACTION_SET_CONFIG,
  USB_ACTION_CLEAR_HALT, /* reset the data toggle of endpoint `ep` */
} USB_Action;

typedef struct {
  USB_ReplyKind kind;
  USB_Action action;
  const uint8_t *data;
  uint16_t len; /* IN: clipped to wLength; OUT: bytes expected */
  bool zlp;     /* IN: end with a zero-length packet */
  uint8_t ep;   /* CLEAR_HALT: endpoint address */
} USB_Reply;

typedef struct {
  uint8_t address;
  uint8_t config;
  uint16_t line_state;    /* bit 0 DTR, bit 1 RTS */
  uint8_t line_coding[7]; /* baud LE32, stop bits, parity, data bits */
  char serial[25];        /* UID as 24 hex digits */
  uint8_t scratch[64];    /* built descriptors and small replies */
} USB_Device;

void hal_usb_calc_device_init(USB_Device *dev, const uint32_t uid[3]);
USB_Reply hal_usb_calc_setup(USB_Device *dev, const USB_Setup *s);
void hal_usb_calc_setup_out(USB_Device *dev, const USB_Setup *s,
                            const uint8_t *data, uint16_t len);
USB_Setup hal_usb_calc_parse_setup(const uint8_t raw[8]);

#endif /* HAL_USB_H */
