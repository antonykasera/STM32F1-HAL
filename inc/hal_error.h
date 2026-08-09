/**
 * @file hal_error.h
 * @brief Shared error status codes + assertion hook
 *
 * Copyright (c) 2026 Flex HAL Project.
 * All rights reserved.
 *
 * HAL_ASSERT(cond) routes a failed check to hal_error_handler(), which is a
 * weak symbol the application may override (e.g to blink an LED, log over
 * UART, or hit a breakpoint). The default handler halts on target and aborts on
 * host. Define HAL_DISABLE_ASSERT to compile the checks out for a release
 * build.
 */

#ifndef HAL_ERROR_H
#define HAL_ERROR_H

#include "hal_types.h"

typedef enum {
  HAL_OK = 0,
  HAL_ERR_PARAM,
  HAL_ERR_UNSUPPORTED,
  HAL_ERR_TIMEOUT,
} HAL_Status;

const char *hal_status_str(HAL_Status status);

void hal_error_handler(HAL_Status status, const char *file, int line);

#ifdef HAL_DISABLE_ASSERT
#define HAL_ASSERT(cond) ((void)0)
#else
#define HAL_ASSERT(cond)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      hal_error_handler(HAL_ERR_PARAM, __FILE__, __LINE__);                    \
    }                                                                          \
  } while (0)
#endif

#endif /* HAL_ERROR_H */
