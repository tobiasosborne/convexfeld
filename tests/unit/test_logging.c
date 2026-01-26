/**
 * @file test_logging.c
 * @brief TDD tests for logging functions (M3.2.1)
 *
 * Tests for logging module functions:
 * - cxf_log10_wrapper
 * - cxf_snprintf_wrapper
 * - cxf_log_printf (basic functionality)
 * - cxf_register_log_callback
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations for logging functions */
double cxf_log10_wrapper(double value);
int cxf_snprintf_wrapper(char *buffer, size_t size, const char *format, ...);
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);
int cxf_register_log_callback(CxfEnv *env,
                               void (*callback)(const char *msg, void *data),
                               void *data);

/* Test callback state */
static char last_callback_msg[256];
static int callback_count;

static void test_log_callback(const char *msg, void *data) {
    (void)data;
    strncpy(last_callback_msg, msg, sizeof(last_callback_msg) - 1);
    last_callback_msg[sizeof(last_callback_msg) - 1] = '\0';
    callback_count++;
}

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
void cxf_freeenv(CxfEnv *env);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    last_callback_msg[0] = '\0';
    callback_count = 0;
}

void tearDown(void) {
    if (env) {
        cxf_freeenv(env);
        env = NULL;
    }
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

void test_snprintf_wrapper_empty_buffer(void) {
    char buffer[1] = {0};
    int result = cxf_snprintf_wrapper(buffer, 0, "test");
    TEST_ASSERT_GREATER_OR_EQUAL(0, result);  /* Does not crash */
}

void test_snprintf_wrapper_null_buffer(void) {
    /* Should return required length without crashing */
    int result = cxf_snprintf_wrapper(NULL, 0, "test string");
    TEST_ASSERT_EQUAL_INT(11, result);
}

/*============================================================================
 * cxf_log_printf Tests
 *===========================================================================*/

void test_log_printf_null_env_safe(void) {
    /* Should not crash with NULL env */
    cxf_log_printf(NULL, 0, "test message");
    TEST_PASS();
}

void test_log_printf_null_format_safe(void) {
    /* Should not crash with NULL format */
    cxf_log_printf(env, 0, NULL);
    TEST_PASS();
}

void test_log_printf_with_callback(void) {
    cxf_register_log_callback(env, test_log_callback, NULL);
    cxf_log_printf(env, 0, "hello world");
    TEST_ASSERT_EQUAL_STRING("hello world", last_callback_msg);
    TEST_ASSERT_EQUAL_INT(1, callback_count);
}

void test_log_printf_format_args(void) {
    cxf_register_log_callback(env, test_log_callback, NULL);
    cxf_log_printf(env, 0, "value=%d, pi=%.2f", 42, 3.14);
    TEST_ASSERT_EQUAL_STRING("value=42, pi=3.14", last_callback_msg);
}

void test_log_printf_verbosity_filtered(void) {
    cxf_register_log_callback(env, test_log_callback, NULL);
    env->verbosity = 0;  /* Silent mode */
    cxf_log_printf(env, 1, "this should not appear");
    TEST_ASSERT_EQUAL_INT(0, callback_count);
}

void test_log_printf_output_flag_disabled(void) {
    cxf_register_log_callback(env, test_log_callback, NULL);
    env->output_flag = 0;  /* Disable output */
    cxf_log_printf(env, 0, "this should not appear");
    TEST_ASSERT_EQUAL_INT(0, callback_count);
}

/*============================================================================
 * cxf_register_log_callback Tests
 *===========================================================================*/

void test_register_log_callback_success(void) {
    int result = cxf_register_log_callback(env, test_log_callback, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_register_log_callback_null_env(void) {
    int result = cxf_register_log_callback(NULL, test_log_callback, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_register_log_callback_unregister(void) {
    cxf_register_log_callback(env, test_log_callback, NULL);
    cxf_log_printf(env, 0, "first");
    TEST_ASSERT_EQUAL_INT(1, callback_count);

    /* Unregister by passing NULL */
    cxf_register_log_callback(env, NULL, NULL);
    cxf_log_printf(env, 0, "second");
    TEST_ASSERT_EQUAL_INT(1, callback_count);  /* Still 1 */
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
    RUN_TEST(test_snprintf_wrapper_empty_buffer);
    RUN_TEST(test_snprintf_wrapper_null_buffer);

    /* cxf_log_printf tests */
    RUN_TEST(test_log_printf_null_env_safe);
    RUN_TEST(test_log_printf_null_format_safe);
    RUN_TEST(test_log_printf_with_callback);
    RUN_TEST(test_log_printf_format_args);
    RUN_TEST(test_log_printf_verbosity_filtered);
    RUN_TEST(test_log_printf_output_flag_disabled);

    /* cxf_register_log_callback tests */
    RUN_TEST(test_register_log_callback_success);
    RUN_TEST(test_register_log_callback_null_env);
    RUN_TEST(test_register_log_callback_unregister);

    return UNITY_END();
}
