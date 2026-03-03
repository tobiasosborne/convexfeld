/**
 * @file test_error_core.c
 * @brief Tests for core error functions: cxf_error, cxf_set_error_string,
 *        cxf_checkenv, cxf_validate_pivot_element, cxf_check_terminate
 *
 * Split from test_error.c. Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <math.h>

/* Forward declarations for functions under test */
void cxf_error(CxfEnv *env, const char *format, ...);
void cxf_set_error_string(CxfEnv *env, const char *message);
int cxf_validate_pivot_element(double pivot_elem, double tolerance);
int cxf_check_terminate(CxfEnv *env);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/*============================================================================
 * cxf_error Tests
 *===========================================================================*/

void test_error_basic_message(void) {
    cxf_error(env, "Test error message");
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Test error message", msg);
}

void test_error_formatted_message(void) {
    cxf_error(env, "Error code %d: %s", 42, "invalid value");
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Error code 42: invalid value", msg);
}

void test_error_null_env_safe(void) {
    cxf_error(NULL, "This should not crash");
    TEST_PASS();
}

void test_error_empty_message(void) {
    cxf_error(env, "");
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_geterrormsg_null_env(void) {
    const char *msg = cxf_geterrormsg(NULL);
    TEST_ASSERT_NOT_NULL(msg);
    /* Should return empty string or some default */
}

/*============================================================================
 * cxf_set_error_string Tests
 *===========================================================================*/

void test_errorlog_null_env(void) {
    cxf_set_error_string(NULL, "message");
    TEST_PASS();  /* Should not crash */
}

void test_errorlog_null_message(void) {
    cxf_set_error_string(env, NULL);
    TEST_PASS();  /* Should not crash */
}

void test_errorlog_basic(void) {
    cxf_set_error_string(env, "Test log message");
    TEST_PASS();  /* Basic call should not crash */
}

/*============================================================================
 * cxf_checkenv Tests
 *===========================================================================*/

void test_checkenv_valid(void) {
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_checkenv(env));
}

void test_checkenv_null(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_checkenv(NULL));
}

void test_checkenv_invalid_magic(void) {
    CxfEnv fake_env;
    fake_env.magic = 0xDEADBEEF;
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_checkenv(&fake_env));
}

/*============================================================================
 * cxf_validate_pivot_element Tests
 *===========================================================================*/

void test_pivot_check_valid(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(1.0, 1e-10));
}

void test_pivot_check_too_small(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(1e-12, 1e-10));
}

void test_pivot_check_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(0.0, 1e-10));
}

void test_pivot_check_negative(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(-1.0, 1e-10));
}

void test_pivot_check_nan(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(NAN, 1e-10));
}

/*============================================================================
 * cxf_check_terminate Tests
 *===========================================================================*/

void test_check_terminate_null_env(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_terminate(NULL));
}

void test_check_terminate_not_set(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_terminate(env));
}

void test_check_terminate_after_terminate(void) {
    cxf_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, cxf_check_terminate(env));
}

void test_check_terminate_after_clear(void) {
    cxf_terminate(env);
    cxf_reset_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, cxf_check_terminate(env));
}

void test_terminate_null_env_safe(void) {
    cxf_terminate(NULL);
    TEST_PASS();
}

void test_clear_terminate_null_env_safe(void) {
    cxf_reset_terminate(NULL);
    TEST_PASS();
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_error_basic_message);
    RUN_TEST(test_error_formatted_message);
    RUN_TEST(test_error_null_env_safe);
    RUN_TEST(test_error_empty_message);
    RUN_TEST(test_geterrormsg_null_env);

    RUN_TEST(test_errorlog_null_env);
    RUN_TEST(test_errorlog_null_message);
    RUN_TEST(test_errorlog_basic);

    RUN_TEST(test_checkenv_valid);
    RUN_TEST(test_checkenv_null);
    RUN_TEST(test_checkenv_invalid_magic);

    RUN_TEST(test_pivot_check_valid);
    RUN_TEST(test_pivot_check_too_small);
    RUN_TEST(test_pivot_check_zero);
    RUN_TEST(test_pivot_check_negative);
    RUN_TEST(test_pivot_check_nan);

    RUN_TEST(test_check_terminate_null_env);
    RUN_TEST(test_check_terminate_not_set);
    RUN_TEST(test_check_terminate_after_terminate);
    RUN_TEST(test_check_terminate_after_clear);
    RUN_TEST(test_terminate_null_env_safe);
    RUN_TEST(test_clear_terminate_null_env_safe);

    return UNITY_END();
}
