/**
 * @file hal_usb_desc.c
 * @brief CDC-ACM descriptors and control request decoding (no registers)
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 * USB 2.0 ch. 9 standard requests, CDC 1.2 PSTN class requests.
 */

#include "hal_usb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LO(x) ((uint8_t)((x) & 0xFFU))
#define HI(x) ((uint8_t)(((x) >> 8) & 0xFFU))

#define REQ_IN 0x80U
#define REQ_TYPE_MASK 0x60U
#define REQ_STANDARD 0x00U
#define REQ_CLASS 0x20U
#define REQ_RECIP_MASK 0x1FU
#define RECIP_DEVICE 0U
#define RECIP_INTERFACE 1U
#define RECIP_ENDPOINT 2U

#define GET_STATUS 0x00U
#define CLEAR_FEATURE 0x01U
#define SET_ADDRESS 0x05U
#define GET_DESCRIPTOR 0x06U
#define GET_CONFIGURATION 0x08U
#define SET_CONFIGURATION 0x09U
#define GET_INTERFACE 0x0AU
#define SET_INTERFACE 0x0BU
#define FEATURE_ENDPOINT_HALT 0U

#define CDC_SET_LINE_CODING 0x20U
#define CDC_GET_LINE_CODING 0x21U
#define CDC_SET_CONTROL_LINE_STATE 0x22U
#define CDC_SEND_BREAK 0x23U
#define LINE_CODING_LEN 7U

#define DESC_DEVICE 1U
#define DESC_CONFIG 2U
#define DESC_STRING 3U

#define EP_NOTIFY 0x82U
#define EP_DATA_OUT 0x01U
#define EP_DATA_IN 0x81U

/* class 02/02 at device level: what Windows' usbser binds to without an IAD */
static const uint8_t device_desc[] = {
    18, DESC_DEVICE, 0x00, 0x02, /* USB 2.0 (full speed only) */
    0x02, 0x02, 0x00,            /* CDC */
    USB_EP0_SIZE,
    LO(USB_VID), HI(USB_VID), LO(USB_PID), HI(USB_PID),
    0x00, 0x01, /* bcdDevice 1.00 */
    1, 2, 3,    /* manufacturer, product, serial strings */
    1,
};

static const uint8_t config_desc[] = {
    9, DESC_CONFIG, 67, 0, 2, 1, 0, 0x80, 50, /* bus powered, 100 mA */

    /* interface 0: ACM control. Protocol 0, not AT commands, so
     * ModemManager leaves the port alone */
    9, 4, 0, 0, 1, 0x02, 0x02, 0x00, 0,
    5, 0x24, 0x00, 0x10, 0x01, /* header, CDC 1.10 */
    5, 0x24, 0x01, 0x00, 1,    /* call management: data on interface 1 */
    4, 0x24, 0x02, 0x02,       /* ACM: line coding + control line state */
    5, 0x24, 0x06, 0, 1,       /* union: control 0, data 1 */
    7, 5, EP_NOTIFY, 0x03, USB_CDC_NOTIFY_SIZE, 0, 255,

    /* interface 1: data */
    9, 4, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
    7, 5, EP_DATA_OUT, 0x02, USB_CDC_DATA_SIZE, 0, 0,
    7, 5, EP_DATA_IN, 0x02, USB_CDC_DATA_SIZE, 0, 0,
};

_Static_assert(sizeof device_desc == 18U, "device descriptor length");
_Static_assert(sizeof config_desc == 67U, "wTotalLength in config_desc[2]");
_Static_assert(sizeof(USB_MANUFACTURER) <= 32U, "USB_MANUFACTURER too long");
_Static_assert(sizeof(USB_PRODUCT) <= 32U, "USB_PRODUCT too long");

/* ASCII to a UTF-16LE string descriptor */
static uint16_t build_string(uint8_t *out, const char *s) {
  uint16_t n = 2U;
  while ((*s != '\0') && (n < 64U)) {
    out[n] = (uint8_t)*s;
    out[n + 1U] = 0U;
    n = (uint16_t)(n + 2U);
    s++;
  }
  out[0] = (uint8_t)n;
  out[1] = DESC_STRING;
  return n;
}

static USB_Reply reply(USB_ReplyKind kind) {
  USB_Reply r = {kind, USB_ACTION_NONE, NULL, 0U, false, 0U};
  return r;
}

static USB_Reply reply_in(const USB_Setup *s, const uint8_t *data,
                          uint16_t len) {
  USB_Reply r = reply(USB_REPLY_IN);
  r.data = data;
  r.len = (len < s->wLength) ? len : s->wLength;
  /* a short transfer needs a short packet to end it */
  r.zlp = (r.len < s->wLength) && ((r.len % USB_EP0_SIZE) == 0U);
  return r;
}

static USB_Reply get_descriptor(USB_Device *dev, const USB_Setup *s) {
  uint8_t index = LO(s->wValue);

  switch (HI(s->wValue)) {
  case DESC_DEVICE:
    return reply_in(s, device_desc, sizeof device_desc);
  case DESC_CONFIG:
    return reply_in(s, config_desc, sizeof config_desc);
  case DESC_STRING:
    if (index == 0U) {
      static const uint8_t langid[4] = {4, DESC_STRING, 0x09, 0x04}; /* en-US */
      return reply_in(s, langid, sizeof langid);
    }
    if (index <= 3U) {
      const char *str = (index == 1U)   ? USB_MANUFACTURER
                        : (index == 2U) ? USB_PRODUCT
                                        : dev->serial;
      return reply_in(s, dev->scratch, build_string(dev->scratch, str));
    }
    break;
  default:
    break; /* incl. device qualifier: full-speed only devices stall it */
  }
  return reply(USB_REPLY_STALL);
}

static USB_Reply class_request(USB_Device *dev, const USB_Setup *s, bool in) {
  switch (s->bRequest) {
  case CDC_SET_LINE_CODING:
    if (!in && (s->wLength == LINE_CODING_LEN)) {
      USB_Reply r = reply(USB_REPLY_OUT);
      r.len = LINE_CODING_LEN;
      return r;
    }
    break;
  case CDC_GET_LINE_CODING:
    if (in) {
      return reply_in(s, dev->line_coding, LINE_CODING_LEN);
    }
    break;
  case CDC_SET_CONTROL_LINE_STATE:
    dev->line_state = s->wValue;
    return reply(USB_REPLY_STATUS);
  case CDC_SEND_BREAK:
    return reply(USB_REPLY_STATUS);
  default:
    break;
  }
  return reply(USB_REPLY_STALL);
}

static USB_Reply clear_feature(const USB_Setup *s, uint8_t recip) {
  uint8_t ep = LO(s->wIndex);
  if ((recip != RECIP_ENDPOINT) || (s->wValue != FEATURE_ENDPOINT_HALT)) {
    return reply(USB_REPLY_STALL);
  }
  USB_Reply r = reply(USB_REPLY_STATUS);
  if ((ep == EP_DATA_OUT) || (ep == EP_DATA_IN) || (ep == EP_NOTIFY)) {
    r.action = USB_ACTION_CLEAR_HALT;
    r.ep = ep;
  } else if ((ep & 0x7FU) != 0U) {
    r = reply(USB_REPLY_STALL);
  }
  return r;
}

USB_Reply hal_usb_calc_setup(USB_Device *dev, const USB_Setup *s) {
  uint8_t type = s->bmRequestType & REQ_TYPE_MASK;
  uint8_t recip = s->bmRequestType & REQ_RECIP_MASK;
  bool in = (s->bmRequestType & REQ_IN) != 0U;
  USB_Reply r = reply(USB_REPLY_STALL);

  if ((type == REQ_CLASS) && (recip == RECIP_INTERFACE)) {
    return class_request(dev, s, in);
  }
  if (type != REQ_STANDARD) {
    return r;
  }

  switch (s->bRequest) {
  case GET_DESCRIPTOR:
    if (in) {
      r = get_descriptor(dev, s);
    }
    break;
  case SET_ADDRESS:
    if (!in && (recip == RECIP_DEVICE) && (s->wValue <= 127U)) {
      dev->address = (uint8_t)s->wValue;
      r = reply(USB_REPLY_STATUS);
      r.action = USB_ACTION_SET_ADDRESS;
    }
    break;
  case GET_CONFIGURATION:
    if (in) {
      dev->scratch[0] = dev->config;
      r = reply_in(s, dev->scratch, 1U);
    }
    break;
  case SET_CONFIGURATION:
    if (!in && (s->wValue <= 1U)) {
      dev->config = (uint8_t)s->wValue;
      r = reply(USB_REPLY_STATUS);
      r.action = USB_ACTION_SET_CONFIG;
    }
    break;
  case GET_STATUS:
    if (in) {
      dev->scratch[0] = 0U; /* bus powered, no remote wake-up, not halted */
      dev->scratch[1] = 0U;
      r = reply_in(s, dev->scratch, 2U);
    }
    break;
  case CLEAR_FEATURE:
    if (!in) {
      r = clear_feature(s, recip);
    }
    break;
  case GET_INTERFACE:
    if (in && (recip == RECIP_INTERFACE)) {
      dev->scratch[0] = 0U;
      r = reply_in(s, dev->scratch, 1U);
    }
    break;
  case SET_INTERFACE:
    if (!in && (recip == RECIP_INTERFACE) && (s->wValue == 0U)) {
      r = reply(USB_REPLY_STATUS);
    }
    break;
  default:
    break;
  }
  return r;
}

void hal_usb_calc_setup_out(USB_Device *dev, const USB_Setup *s,
                            const uint8_t *data, uint16_t len) {
  if ((s->bRequest == CDC_SET_LINE_CODING) && (len == LINE_CODING_LEN)) {
    for (uint16_t i = 0U; i < LINE_CODING_LEN; i++) {
      dev->line_coding[i] = data[i];
    }
  }
}

USB_Setup hal_usb_calc_parse_setup(const uint8_t raw[8]) {
  USB_Setup s = {
      raw[0],
      raw[1],
      (uint16_t)(raw[2] | (raw[3] << 8)),
      (uint16_t)(raw[4] | (raw[5] << 8)),
      (uint16_t)(raw[6] | (raw[7] << 8)),
  };
  return s;
}

void hal_usb_calc_device_init(USB_Device *dev, const uint32_t uid[3]) {
  static const char hex[] = "0123456789ABCDEF";
  static const uint8_t coding[LINE_CODING_LEN] = {0x00, 0xC2, 0x01, 0x00,
                                                  0,    0,    8}; /* 115200 8N1 */
  dev->address = 0U;
  dev->config = 0U;
  dev->line_state = 0U;
  for (uint32_t i = 0U; i < LINE_CODING_LEN; i++) {
    dev->line_coding[i] = coding[i];
  }
  for (uint32_t i = 0U; i < 24U; i++) {
    dev->serial[i] = hex[(uid[i / 8U] >> (28U - (4U * (i % 8U)))) & 0xFU];
  }
  dev->serial[24] = '\0';
}
