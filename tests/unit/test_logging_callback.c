/**
 * @file test_logging_callback.c
 * @brief TDD tests for logging callback and system functions (M3.2.1)
 *
 * Tests for:
 * - cxf_log_printf (basic functionality)
 * - cxf_register_log_callback
 * - cxf_get_logical_processors
 *
 * Split from test_logging.c for 200 LOC compliance.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations for logging functions */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);
int cxf_register_log_callback(CxfEnv *env,
                               void (*callback)(const char *msg, void *data),
                               void *data);
int cxf_get_logical_processors(void);

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
int cxf_freeenv(CxfEnv *env);

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
 * cxf_get_logical_processors Tests
 *===========================================================================*/

void test_get_logical_processors_returns_positive(void) {
    int count = cxf_get_logical_processors();
    TEST_ASSERT_GREATER_THAN(0, count);
}

void test_get_logical_processors_returns_at_least_one(void) {
    int count = cxf_get_logical_processors();
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
}

void test_get_logical_processors_reasonable_range(void) {
    int count = cxf_get_logical_processors();
    /* Should be between 1 and 1024 (reasonable server max) */
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
    TEST_ASSERT_LESS_OR_EQUAL(1024, count);
}

void test_get_logical_processors_consistent(void) {
    /* Multiple calls should return the same value */
    int first = cxf_get_logical_processors();
    int second = cxf_get_logical_processors();
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

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

    /* cxf_get_logical_processors tests */
    RUN_TEST(test_get_logical_processors_returns_positive);
    RUN_TEST(test_get_logical_processors_returns_at_least_one);
    RUN_TEST(test_get_logical_processors_reasonable_range);
    RUN_TEST(test_get_logical_processors_consistent);

    return UNITY_END();
}
