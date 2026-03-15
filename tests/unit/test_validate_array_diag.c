/**
 * @file test_validate_array_diag.c
 * @brief Tests for cxf_validate_array diagnostic error messages.
 *
 * Verifies that cxf_validate_array writes a diagnostic message to the
 * environment error buffer on NaN detection, per data_validation.md V2.
 * Covers: message content, index reporting, cascading error pattern
 * (empty-buffer-only), NULL env safety, and error_code correctness.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>

/* Forward declaration */
int cxf_validate_array(CxfEnv *env, int count, const double *array);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_clearerrormsg(env);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*==========================================================================
 * Diagnostic message tests
 *=========================================================================*/

/** NaN at index 0 writes message identifying index 0. */
void test_nan_at_index_0_writes_message(void) {
    double arr[] = {NAN, 1.0, 2.0};
    int rc = cxf_validate_array(env, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
    /* Message must mention index 0 */
    TEST_ASSERT_NOT_NULL(strstr(cxf_geterrormsg(env), "0"));
    TEST_ASSERT_NOT_NULL(strstr(cxf_geterrormsg(env), "NaN"));
}

/** NaN at middle index writes correct index in message. */
void test_nan_at_middle_index_writes_message(void) {
    double arr[] = {1.0, 2.0, NAN, 4.0, 5.0};
    int rc = cxf_validate_array(env, 5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
    /* Message must identify index 2 */
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(strstr(msg, "NaN"));
    TEST_ASSERT_NOT_NULL(strstr(msg, "2"));
}

/** NaN at last index writes correct index in message. */
void test_nan_at_last_index_writes_message(void) {
    double arr[] = {1.0, 2.0, 3.0, NAN};
    int rc = cxf_validate_array(env, 4, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(strstr(msg, "NaN"));
    TEST_ASSERT_NOT_NULL(strstr(msg, "3"));
}

/** Valid array leaves error buffer empty. */
void test_valid_array_no_message(void) {
    double arr[] = {1.0, 2.0, 3.0};
    int rc = cxf_validate_array(env, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* Buffer must remain empty */
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
}

/** NULL env does not crash on NaN. */
void test_null_env_no_crash(void) {
    double arr[] = {NAN};
    int rc = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

/** Cascading error: pre-existing message is preserved (overwrite=0). */
void test_cascading_error_preserves_first_message(void) {
    /* Simulate a prior error already in the buffer */
    cxf_error_env(env, CXF_ERROR_NULL_ARGUMENT, 0, "root cause");
    double arr[] = {NAN};
    int rc = cxf_validate_array(env, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
    /* Original message preserved per cascading error pattern */
    TEST_ASSERT_EQUAL_STRING("root cause", cxf_geterrormsg(env));
    /* But error_code is always updated */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
}

/** Multiple NaN: only first index reported (early-exit). */
void test_multiple_nan_reports_first_only(void) {
    double arr[] = {1.0, NAN, NAN, NAN};
    int rc = cxf_validate_array(env, 4, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(strstr(msg, "1"));
}

/** Infinity values do not trigger error or message. */
void test_infinity_no_message(void) {
    double arr[] = {INFINITY, -INFINITY, 1.0};
    int rc = cxf_validate_array(env, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_nan_at_index_0_writes_message);
    RUN_TEST(test_nan_at_middle_index_writes_message);
    RUN_TEST(test_nan_at_last_index_writes_message);
    RUN_TEST(test_valid_array_no_message);
    RUN_TEST(test_null_env_no_crash);
    RUN_TEST(test_cascading_error_preserves_first_message);
    RUN_TEST(test_multiple_nan_reports_first_only);
    RUN_TEST(test_infinity_no_message);

    return UNITY_END();
}
