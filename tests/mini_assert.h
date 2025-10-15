
#ifndef MINI_ASSERT_H
#define MINI_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

static int _tests_run = 0;
static int _tests_failed = 0;

#define ASSERT_TRUE(msg, cond) do { \
    _tests_run++; \
    if (!(cond)) { \
        _tests_failed++; \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return -1; \
    } \
} while (0)

#define ASSERT_EQ_INT(msg, a, b) do { \
    _tests_run++; \
    if ((int)(a) != (int)(b)) { \
        _tests_failed++; \
        fprintf(stderr, "[FAIL] %s:%d: %s (got %d, expected %d)\n", __FILE__, __LINE__, msg, (int)(a), (int)(b)); \
        return -1; \
    } \
} while (0)

#define TEST(name) static int name(void)

#define RUN_TEST(fn) do { \
    int _r = fn(); \
    if (_r == 0) { \
        printf("[ OK ] %s\n", #fn); \
    } else { \
        printf("[FAIL] %s\n", #fn); \
    } \
} while (0)

#define TEST_SUMMARY() do { \
    if (_tests_failed == 0) { \
        printf("\nAll tests passed (%d checks).\n", _tests_run); \
        return 0; \
    } else { \
        printf("\n%d tests failed out of %d checks.\n", _tests_failed, _tests_run); \
        return 1; \
    } \
} while (0)

#endif
