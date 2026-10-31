/**
 * @file hal_error.c
 * @brief Shared error handling utilities
 *
 * Copyright (c) 2026.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_error.h"

#ifdef HAL_TEST_HOST
#include <stdio.h>
#include <stdlib.h>
#else
#include "stm32f103xb.h"
#endif

const char *hal_status_str(HAL_Status status) {
  switch (status) {
  case HAL_OK:
    return "HAL_OK";
  case HAL_ERR_PARAM:
    return "HAL_ERR_PARAM";
  case HAL_ERR_UNSUPPORTED:
    return "HAL_ERR_UNSUPPORTED";
  case HAL_ERR_TIMEOUT:
    return "HAL_ERR_TIMEOUT";
  case HAL_ERR_NACK:
    return "HAL_ERR_NACK";
  case HAL_ERR_BUS:
    return "HAL_ERR_BUS";
  case HAL_ERR_EMPTY:
    return "HAL_ERR_EMPTY";
  case HAL_ERR_BUSY:
    return "HAL_ERR_BUSY";
  }
  return "HAL_ERR_UNKNOWN";
}

__attribute__((weak)) void hal_error_handler(HAL_Status status,
                                             const char *file, int line) {
#ifdef HAL_TEST_HOST
  fprintf(stderr, "HAL error %s at %s:%d\n", hal_status_str(status), file,
          line);
  abort();
#else
  (void)status;
  (void)file;
  (void)line;
  __disable_irq();
  for (;;) {
    /* halt; override hal_error_handler() in the app to blink/log/breakpoint */
  }
#endif
}
