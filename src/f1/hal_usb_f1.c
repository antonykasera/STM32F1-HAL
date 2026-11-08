/**
 * @file hal_usb_f1.c
 * @brief STM32F1 USB full-speed device driver, CDC-ACM
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Every endpoint register access happens in usb_service(), from
 * hal_usb_poll() or the IRQ, never both: in IRQ mode read/write just pend it.
 */

#include "hal_clock.h"
#include "hal_error.h"
#include "hal_gpio.h"
#include "hal_usb.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert((USB_CDC_RX_SIZE & (USB_CDC_RX_SIZE - 1U)) == 0U, "power of 2");
_Static_assert((USB_CDC_TX_SIZE & (USB_CDC_TX_SIZE - 1U)) == 0U, "power of 2");
_Static_assert(USB_CDC_RX_SIZE >= USB_CDC_DATA_SIZE, "RX ring < one packet");

/* packet memory: USB-side address a sits at 0x40006000 + 2a on APB */
#define PMA ((volatile uint16_t *)USB_PMAADDR)
#define PMA_EP0_RX 0x040U /* 0x000-0x017: buffer table, 3 endpoints */
#define PMA_EP0_TX 0x080U
#define PMA_EP1_RX 0x0C0U
#define PMA_EP1_TX 0x100U
#define PMA_EP2_TX 0x140U

#define BT_ADDR_TX 0U
#define BT_COUNT_TX 1U
#define BT_ADDR_RX 2U
#define BT_COUNT_RX 3U
#define COUNT_RX_MASK 0x3FFU

#define EP_DATA 1U   /* 0x01 OUT + 0x81 IN */
#define EP_NOTIFY 2U /* 0x82, never sent */
#define EPR_ALL 0xFFFFU

#define USB_PIN_DP 12U
#define USB_SYSCLK_HZ 72000000U

#define ISTR_OTHER                                                             \
  (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP | USB_ISTR_SUSP |            \
   USB_ISTR_SOF | USB_ISTR_ESOF)

typedef enum { EP0_IDLE, EP0_IN, EP0_OUT, EP0_STATUS_IN } Ep0State;

static USB_Device dev;
static uint8_t rx_mem[USB_CDC_RX_SIZE];
static uint8_t tx_mem[USB_CDC_TX_SIZE];
static USB_Ring rx_ring = {rx_mem, USB_CDC_RX_SIZE, 0U, 0U};
static USB_Ring tx_ring = {tx_mem, USB_CDC_TX_SIZE, 0U, 0U};

static struct {
  Ep0State state;
  const uint8_t *data;
  uint16_t left;
  bool zlp;
  USB_Action after; /* applied once the status IN is acknowledged */
  USB_Setup setup;
} ep0;

static bool tx_busy;      /* a packet is armed on 0x81 */
static bool tx_last_full; /* a 64 B packet went last: owe a ZLP when idle */
static bool rx_held;      /* 0x01 data waits in PMA (NAK) until the ring has room */
static uint16_t rx_held_len;
static volatile bool irq_mode;

static volatile uint16_t *epr(uint8_t ep) { return &USB->EP0R + (2U * ep); }

static void epr_set(uint8_t ep, uint16_t mask, uint16_t want) {
  volatile uint16_t *r = epr(ep);
  *r = hal_usb_calc_epr(*r, mask, want);
}

static volatile uint16_t *btable(uint8_t ep, uint8_t field) {
  return &PMA[(ep * 8U) + (field * 2U)];
}

static void pma_write(uint16_t addr, const uint8_t *src, uint16_t n) {
  for (uint16_t i = 0U; i < n; i = (uint16_t)(i + 2U)) {
    uint16_t v = src[i];
    if ((i + 1U) < n) {
      v = (uint16_t)(v | (src[i + 1U] << 8));
    }
    PMA[addr + i] = v;
  }
}

static void pma_read(uint16_t addr, uint8_t *dst, uint16_t n) {
  for (uint16_t i = 0U; i < n; i = (uint16_t)(i + 2U)) {
    uint16_t v = PMA[addr + i];
    dst[i] = (uint8_t)v;
    if ((i + 1U) < n) {
      dst[i + 1U] = (uint8_t)(v >> 8);
    }
  }
}

/* ---- CDC data endpoints ---- */

static void cdc_configure(bool on) {
  tx_busy = false;
  tx_last_full = false;
  rx_held = false;
  if (!on) {
    epr_set(EP_DATA, USB_EPTX_STAT | USB_EPRX_STAT, 0U); /* disabled */
    epr_set(EP_NOTIFY, USB_EPTX_STAT, 0U);
    return;
  }
  *btable(EP_DATA, BT_ADDR_TX) = PMA_EP1_TX;
  *btable(EP_DATA, BT_COUNT_TX) = 0U;
  *btable(EP_DATA, BT_ADDR_RX) = PMA_EP1_RX;
  *btable(EP_DATA, BT_COUNT_RX) = hal_usb_calc_count_rx(USB_CDC_DATA_SIZE);
  epr_set(EP_DATA, EPR_ALL,
          USB_EP_BULK | EP_DATA | USB_EP_TX_NAK | USB_EP_RX_VALID);

  *btable(EP_NOTIFY, BT_ADDR_TX) = PMA_EP2_TX;
  *btable(EP_NOTIFY, BT_COUNT_TX) = 0U;
  epr_set(EP_NOTIFY, EPR_ALL, USB_EP_INTERRUPT | EP_NOTIFY | USB_EP_TX_NAK);
}

static void cdc_clear_halt(uint8_t ep_addr) {
  if (ep_addr == 0x01U) {
    rx_held = false; /* drops a held packet */
    epr_set(EP_DATA, USB_EP_DTOG_RX | USB_EPRX_STAT, USB_EP_RX_VALID);
  } else if (ep_addr == 0x81U) {
    tx_busy = false;
    epr_set(EP_DATA, USB_EP_DTOG_TX | USB_EPTX_STAT, USB_EP_TX_NAK);
  } else {
    epr_set(EP_NOTIFY, USB_EP_DTOG_TX | USB_EPTX_STAT, USB_EP_TX_NAK);
  }
}

static void cdc_drain_rx(void) {
  if (!rx_held || (hal_usb_ring_free(&rx_ring) < rx_held_len)) {
    return;
  }
  uint8_t buf[USB_CDC_DATA_SIZE];
  pma_read(PMA_EP1_RX, buf, rx_held_len);
  (void)hal_usb_ring_write(&rx_ring, buf, rx_held_len);
  rx_held = false;
  epr_set(EP_DATA, USB_EPRX_STAT, USB_EP_RX_VALID);
}

static void cdc_kick_tx(void) {
  if ((dev.config == 0U) || tx_busy) {
    return;
  }
  uint8_t buf[USB_CDC_DATA_SIZE];
  uint16_t n = (uint16_t)hal_usb_ring_read(&tx_ring, buf, sizeof buf);
  if ((n == 0U) && !tx_last_full) {
    return;
  }
  pma_write(PMA_EP1_TX, buf, n);
  *btable(EP_DATA, BT_COUNT_TX) = n;
  tx_last_full = (n == USB_CDC_DATA_SIZE);
  tx_busy = true;
  epr_set(EP_DATA, USB_EPTX_STAT, USB_EP_TX_VALID);
}

static void cdc_service(uint16_t r) {
  if ((r & USB_EP_CTR_RX) != 0U) {
    epr_set(EP_DATA, USB_EP_CTR_RX, 0U);
    rx_held_len = (uint16_t)(*btable(EP_DATA, BT_COUNT_RX) & COUNT_RX_MASK);
    if (rx_held_len > USB_CDC_DATA_SIZE) {
      rx_held_len = USB_CDC_DATA_SIZE;
    }
    rx_held = true;
  }
  if ((r & USB_EP_CTR_TX) != 0U) {
    epr_set(EP_DATA, USB_EP_CTR_TX, 0U);
    tx_busy = false;
  }
}

/* ---- endpoint 0: control transfers ---- */

static void ep0_send_chunk(void) {
  uint16_t n = (ep0.left < USB_EP0_SIZE) ? ep0.left : (uint16_t)USB_EP0_SIZE;
  if (n > 0U) {
    pma_write(PMA_EP0_TX, ep0.data, n);
    ep0.data += n;
    ep0.left = (uint16_t)(ep0.left - n);
  } else {
    ep0.zlp = false;
  }
  *btable(0U, BT_COUNT_TX) = n;
  epr_set(0U, USB_EPTX_STAT, USB_EP_TX_VALID);
}

static void ep0_status_in(USB_Action after) {
  ep0.state = EP0_STATUS_IN;
  ep0.after = after;
  *btable(0U, BT_COUNT_TX) = 0U;
  epr_set(0U, USB_EPTX_STAT, USB_EP_TX_VALID);
}

static void ep0_setup(void) {
  uint8_t raw[8];
  pma_read(PMA_EP0_RX, raw, sizeof raw);
  epr_set(0U, USB_EP_CTR_RX, 0U);

  ep0.setup = hal_usb_calc_parse_setup(raw);
  USB_Reply r = hal_usb_calc_setup(&dev, &ep0.setup);

  switch (r.kind) {
  case USB_REPLY_IN:
    ep0.state = EP0_IN;
    ep0.data = r.data;
    ep0.left = r.len;
    ep0.zlp = r.zlp;
    ep0_send_chunk();
    break;
  case USB_REPLY_OUT:
    ep0.state = EP0_OUT;
    break;
  case USB_REPLY_STATUS:
    if (r.action == USB_ACTION_SET_CONFIG) {
      cdc_configure(dev.config != 0U);
    } else if (r.action == USB_ACTION_CLEAR_HALT) {
      cdc_clear_halt(r.ep);
    }
    ep0_status_in(r.action);
    break;
  default:
    /* the next SETUP is still accepted: the hardware never stalls those */
    ep0.state = EP0_IDLE;
    epr_set(0U, USB_EPTX_STAT | USB_EPRX_STAT,
            USB_EP_TX_STALL | USB_EP_RX_STALL);
    return;
  }
  epr_set(0U, USB_EPRX_STAT, USB_EP_RX_VALID);
}

static void ep0_out(void) {
  uint16_t n = (uint16_t)(*btable(0U, BT_COUNT_RX) & COUNT_RX_MASK);
  epr_set(0U, USB_EP_CTR_RX, 0U);

  if (ep0.state == EP0_OUT) { /* data stage: our OUT requests fit one packet */
    uint8_t buf[USB_EP0_SIZE];
    if (n > USB_EP0_SIZE) {
      n = USB_EP0_SIZE;
    }
    pma_read(PMA_EP0_RX, buf, n);
    hal_usb_calc_setup_out(&dev, &ep0.setup, buf, n);
    ep0_status_in(USB_ACTION_NONE);
  } else {
    ep0.state = EP0_IDLE; /* status OUT after an IN transfer */
  }
  epr_set(0U, USB_EPRX_STAT, USB_EP_RX_VALID);
}

static void ep0_in_done(void) {
  epr_set(0U, USB_EP_CTR_TX, 0U);

  if (ep0.state == EP0_IN) {
    if ((ep0.left > 0U) || ep0.zlp) {
      ep0_send_chunk();
    }
  } else if (ep0.state == EP0_STATUS_IN) {
    if (ep0.after == USB_ACTION_SET_ADDRESS) {
      USB->DADDR = (uint16_t)(USB_DADDR_EF | dev.address);
    }
    ep0.state = EP0_IDLE;
  } else {
    /* nothing armed */
  }
}

/* ---- bus events ---- */

static void usb_bus_reset(void) {
  USB->BTABLE = 0U;
  *btable(0U, BT_ADDR_TX) = PMA_EP0_TX;
  *btable(0U, BT_COUNT_TX) = 0U;
  *btable(0U, BT_ADDR_RX) = PMA_EP0_RX;
  *btable(0U, BT_COUNT_RX) = hal_usb_calc_count_rx(USB_EP0_SIZE);
  epr_set(0U, EPR_ALL, USB_EP_CONTROL | USB_EP_TX_NAK | USB_EP_RX_VALID);
  USB->DADDR = USB_DADDR_EF; /* address 0 */

  dev.address = 0U;
  dev.config = 0U;
  dev.line_state = 0U;
  ep0.state = EP0_IDLE;
  tx_busy = false;
  tx_last_full = false;
  rx_held = false;
}

static void usb_service(void) {
  uint16_t istr = USB->ISTR;

  /* rc_w0: write 0 only to the flags being cleared */
  if ((istr & USB_ISTR_RESET) != 0U) {
    USB->ISTR = (uint16_t)~USB_ISTR_RESET;
    usb_bus_reset();
  }
  if ((istr & ISTR_OTHER) != 0U) {
    USB->ISTR = (uint16_t)~(istr & ISTR_OTHER); /* suspend etc. ignored */
  }

  while (((istr = USB->ISTR) & USB_ISTR_CTR) != 0U) {
    uint8_t ep = (uint8_t)(istr & USB_ISTR_EP_ID);
    uint16_t r = *epr(ep);

    if (ep == 0U) {
      if ((r & USB_EP_CTR_TX) != 0U) { /* first: may apply the new address */
        ep0_in_done();
      }
      if ((r & USB_EP_CTR_RX) != 0U) {
        if ((r & USB_EP_SETUP) != 0U) {
          ep0_setup();
        } else {
          ep0_out();
        }
      }
    } else if (ep == EP_DATA) {
      cdc_service(r);
    } else {
      epr_set(ep, USB_EP_CTR_RX | USB_EP_CTR_TX, 0U);
    }
  }

  cdc_drain_rx();
  cdc_kick_tx();
}

/* ---- public API ---- */

void hal_usb_init(void) {
  HAL_ASSERT(hal_clock_get_sysclk_hz() == USB_SYSCLK_HZ); /* PLL / 1.5 = 48 */

  const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;
  const uint32_t id[3] = {uid[0], uid[1], uid[2]};
  hal_usb_calc_device_init(&dev, id);

  /* D+ low for 10 ms looks like an unplug, so a reset or reflash
   * re-enumerates despite the Blue Pill's fixed pull-up */
  GPIO_Config out = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};
  GPIO_Config in = {GPIO_MODE_INPUT, GPIO_SPEED_INPUT, GPIO_PULL_NONE};
  hal_gpio_init_port_clock(GPIOA);
  hal_gpio_pin_write(GPIOA, USB_PIN_DP, false);
  hal_gpio_pin_init(GPIOA, USB_PIN_DP, &out);
  hal_delay_ms(10U);
  hal_gpio_pin_init(GPIOA, USB_PIN_DP, &in); /* USB takes the pins once on */

  RCC->CFGR &= ~RCC_CFGR_USBPRE; /* 0: PLL / 1.5 */
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;

  USB->CNTR = USB_CNTR_FRES; /* PDWN off: transceiver powers up */
  for (volatile uint32_t i = 0U; i < 72U; i++) {
    /* tSTARTUP: 1 us max (datasheet) */
  }
  USB->CNTR = 0U;
  USB->ISTR = 0U;
  /* the host's bus reset arrives next, and sets up endpoint 0 */
}

void hal_usb_poll(void) {
  if (irq_mode) {
    __NVIC_SetPendingIRQ(USB_LP_IRQn);
  } else {
    usb_service();
  }
}

void hal_usb_enable_interrupt(uint8_t priority) {
  irq_mode = true;
  USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM;
  __NVIC_SetPriority(USB_LP_IRQn, priority);
  __NVIC_EnableIRQ(USB_LP_IRQn);
}

void USB_LP_IRQHandler(void) { usb_service(); }

bool hal_usb_configured(void) {
  return *(volatile uint8_t *)&dev.config != 0U;
}

bool hal_usb_cdc_dtr(void) {
  return (*(volatile uint16_t *)&dev.line_state & 1U) != 0U;
}

uint32_t hal_usb_cdc_baud(void) {
  const volatile uint8_t *c = dev.line_coding;
  return (uint32_t)c[0] | ((uint32_t)c[1] << 8) | ((uint32_t)c[2] << 16) |
         ((uint32_t)c[3] << 24);
}

size_t hal_usb_cdc_write(const void *buf, size_t len) {
  HAL_ASSERT((buf != NULL) || (len == 0U));
  size_t n = hal_usb_ring_write(&tx_ring, buf, len);
  if (irq_mode) {
    __NVIC_SetPendingIRQ(USB_LP_IRQn);
  }
  return n;
}

size_t hal_usb_cdc_read(void *buf, size_t max) {
  HAL_ASSERT((buf != NULL) || (max == 0U));
  size_t n = hal_usb_ring_read(&rx_ring, buf, max);
  if (irq_mode) {
    __NVIC_SetPendingIRQ(USB_LP_IRQn); /* may release a held packet */
  }
  return n;
}
