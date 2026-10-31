/**
 * @file test_error.c
 * @brief Host-side unit tests for hal_error status strings
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 */

#include "hal_error.h"
#include "test_assert.h"
#include <string.h>

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

void test_status_str_known(void) {
  ASSERT_TRUE(streq(hal_status_str(HAL_OK), "HAL_OK"));
  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_PARAM), "HAL_ERR_PARAM"));
  ASSERT_TRUE(
      streq(hal_status_str(HAL_ERR_UNSUPPORTED), "HAL_ERR_UNSUPPORTED"));
  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_TIMEOUT), "HAL_ERR_TIMEOUT"));

  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_NACK), "HAL_ERR_NACK"));
  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_BUS), "HAL_ERR_BUS"));
  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_BUSY), "HAL_ERR_BUSY"));
  ASSERT_TRUE(streq(hal_status_str(HAL_ERR_EMPTY), "HAL_ERR_EMPTY"));
}

void test_status_str_unknown(void) {
  ASSERT_TRUE(streq(hal_status_str((HAL_Status)999), "HAL_ERR_UNKNOWN"));
}

void test_assert_true_is_noop(void) {
  HAL_ASSERT(1 == 1);
  HAL_ASSERT(42 > 0);
  ASSERT_TRUE(1); /* reached here => no abort */
}
