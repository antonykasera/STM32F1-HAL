/**
 * @file test_assert.h
 * @brief Minimal host-side unit-test harness
 *
 * Copyright (c) 2026 Flex HAL Project.
 * SPDX-License-Identifier: MIT
 *
 * Usage:
 *   static void my_test(void) {
 *     ASSERT_EQ(2 + 2, 4);
 *     ASSERT_TRUE(1 < 2);
 *   }
 *   int main(void) {
 *     TEST_RUN(my_test);
 *     return TEST_SUMMARY();
 *   }
 */

#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <stdint.h>
#include <stdio.h>

static unsigned tests_run = 0;
static unsigned tests_failed = 0;
static unsigned checks_failed = 0;

#define ASSERT_EQ(actual, expected)                                            \
  do {                                                                         \
    long _a = (long)(actual);                                                  \
    long _e = (long)(expected);                                                \
    if (_a != _e) {                                                            \
      checks_failed++;                                                         \
      printf("  FAIL %s:%d: %s == %s  (got 0x%lX, want 0x%lX)\n", __FILE__,    \
             __LINE__, #actual, #expected, (unsigned long)_a,                  \
             (unsigned long)_e);                                               \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      checks_failed++;                                                         \
      printf("  FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #cond);  \
    }                                                                          \
  } while (0)

#define TEST_RUN(fn)                                                           \
  do {                                                                         \
    unsigned _before = checks_failed;                                          \
    tests_run++;                                                               \
    printf("RUN  %s\n", #fn);                                                  \
    fn();                                                                      \
    if (checks_failed != _before) {                                            \
      tests_failed++;                                                          \
      printf("FAIL %s\n", #fn);                                                \
    } else {                                                                   \
      printf("PASS %s\n", #fn);                                                \
    }                                                                          \
  } while (0)

static inline int test_summary(void) {
  printf("\n%u tests, %u failed, %u checks failed\n", tests_run, tests_failed,
         checks_failed);
  return tests_failed == 0 ? 0 : 1;
}

#define TEST_SUMMARY() test_summary()

#endif /* TEST_ASSERT_H */
