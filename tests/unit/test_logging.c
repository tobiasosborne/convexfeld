/**
 * @file test_logging.c
 * @brief TDD tests for logging wrapper functions (M3.2.1)
 *
 * Tests for:
 * - cxf_log10_wrapper
 * - cxf_snprintf_wrapper
 *
 * Callback and system tests are in test_logging_callback.c.
 */

#include "unity.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/* Forward declarations for logging functions */
double cxf_log10_wrapper(double value);
int cxf_snprintf_wrapper(char *buffer, size_t size, const char *format, ...);

void setUp(void) {
}

void tearDown(void) {
}

/*============================================================================
 * cxf_log10_wrapper Tests
 *===========================================================================*/

void test_log10_wrapper_one(void) {
    double result = cxf_log10_wrapper(1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result);
}

void test_log10_wrapper_ten(void) {
    double result = cxf_log10_wrapper(10.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result);
}

void test_log10_wrapper_hundred(void) {
    double result = cxf_log10_wrapper(100.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, result);
}

void test_log10_wrapper_fraction(void) {
    double result = cxf_log10_wrapper(0.1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -1.0, result);
}

void test_log10_wrapper_zero_returns_neg_inf(void) {
    double result = cxf_log10_wrapper(0.0);
    TEST_ASSERT_TRUE(isinf(result) && result < 0);
}

void test_log10_wrapper_negative_returns_nan(void) {
    double result = cxf_log10_wrapper(-1.0);
    TEST_ASSERT_TRUE(isnan(result));
}

void test_log10_wrapper_nan_returns_nan(void) {
    double result = cxf_log10_wrapper(NAN);
    TEST_ASSERT_TRUE(isnan(result));
}

void test_log10_wrapper_positive_inf_returns_inf(void) {
    double result = cxf_log10_wrapper(INFINITY);
    TEST_ASSERT_TRUE(isinf(result) && result > 0);
}

void test_log10_wrapper_very_small(void) {
    double result = cxf_log10_wrapper(1e-100);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, -100.0, result);
}

void test_log10_wrapper_very_large(void) {
    double result = cxf_log10_wrapper(1e100);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 100.0, result);
}

/*============================================================================
 * cxf_snprintf_wrapper Tests
 *===========================================================================*/

void test_snprintf_wrapper_basic_string(void) {
    char buffer[64];
    int result = cxf_snprintf_wrapper(buffer, sizeof(buffer), "hello");
    TEST_ASSERT_EQUAL_INT(5, result);
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_snprintf_wrapper_format_int(void) {
    char buffer[64];
    int result = cxf_snprintf_wrapper(buffer, sizeof(buffer), "value=%d", 42);
    TEST_ASSERT_EQUAL_INT(8, result);
    TEST_ASSERT_EQUAL_STRING("value=42", buffer);
}

void test_snprintf_wrapper_format_double(void) {
    char buffer[64];
    cxf_snprintf_wrapper(buffer, sizeof(buffer), "pi=%.2f", 3.14159);
    TEST_ASSERT_EQUAL_STRING("pi=3.14", buffer);
}

void test_snprintf_wrapper_truncation(void) {
    char buffer[8];
    int result = cxf_snprintf_wrapper(buffer, sizeof(buffer), "this is a long string");
    /* Result is length that would be written, buffer is truncated */
    TEST_ASSERT_GREATER_THAN(8, result);
    TEST_ASSERT_EQUAL_CHAR('\0', buffer[7]);  /* Null terminated */
}

void test_snprintf_wrapper_zero_size_returns_error(void) {
    char buffer[16];
    int result = cxf_snprintf_wrapper(buffer, 0, "test");
    TEST_ASSERT_EQUAL_INT(-1, result);  /* Error per spec */
}

void test_snprintf_wrapper_null_buffer_returns_error(void) {
    /* NULL buffer should return -1 per specification */
    int result = cxf_snprintf_wrapper(NULL, 10, "test string");
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_log10_wrapper tests */
    RUN_TEST(test_log10_wrapper_one);
    RUN_TEST(test_log10_wrapper_ten);
    RUN_TEST(test_log10_wrapper_hundred);
    RUN_TEST(test_log10_wrapper_fraction);
    RUN_TEST(test_log10_wrapper_zero_returns_neg_inf);
    RUN_TEST(test_log10_wrapper_negative_returns_nan);
    RUN_TEST(test_log10_wrapper_nan_returns_nan);
    RUN_TEST(test_log10_wrapper_positive_inf_returns_inf);
    RUN_TEST(test_log10_wrapper_very_small);
    RUN_TEST(test_log10_wrapper_very_large);

    /* cxf_snprintf_wrapper tests */
    RUN_TEST(test_snprintf_wrapper_basic_string);
    RUN_TEST(test_snprintf_wrapper_format_int);
    RUN_TEST(test_snprintf_wrapper_format_double);
    RUN_TEST(test_snprintf_wrapper_truncation);
    RUN_TEST(test_snprintf_wrapper_zero_size_returns_error);
    RUN_TEST(test_snprintf_wrapper_null_buffer_returns_error);

    return UNITY_END();
}
