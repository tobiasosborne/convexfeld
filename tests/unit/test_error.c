/**
 * @file test_error.c
 * @brief TDD tests for error handling operations (M3.1.1)
 *
 * Tests for all 10 error handling functions.
 * Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <float.h>

/* Forward declarations for functions under test */
/* Note: API functions are declared in headers, only internal ones here */

/* Core Error Functions (M3.1.2) */
void cxf_error(CxfEnv *env, const char *format, ...);
void cxf_errorlog(CxfEnv *env, const char *message);

/* NaN/Inf Detection (M3.1.3) */
int cxf_check_nan(const double *arr, int n);
int cxf_check_nan_or_inf(const double *arr, int n);

/* Model Flag Checks (M3.1.5) */
int cxf_check_model_flags1(CxfModel *model);
int cxf_check_model_flags2(CxfModel *model, int flag);

/* Termination Check (M3.1.6) */
int cxf_check_terminate(CxfEnv *env);

/* Pivot Validation (M3.1.7) */
int cxf_pivot_check(double pivot_elem, double tolerance);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

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
 * cxf_errorlog Tests
 *===========================================================================*/

void test_errorlog_null_env(void) {
    cxf_errorlog(NULL, "message");
    TEST_PASS();  /* Should not crash */
}

void test_errorlog_null_message(void) {
    cxf_errorlog(env, NULL);
    TEST_PASS();  /* Should not crash */
}

void test_errorlog_basic(void) {
    cxf_errorlog(env, "Test log message");
    TEST_PASS();  /* Basic call should not crash */
}

/*============================================================================
 * cxf_check_nan Tests
 *===========================================================================*/

void test_check_nan_clean_array(void) {
    double arr[] = {1.0, 2.0, 3.0, -4.5, 0.0};
    int result = cxf_check_nan(arr, 5);
    TEST_ASSERT_EQUAL_INT(0, result);  /* No NaN found */
}

void test_check_nan_with_nan(void) {
    double arr[] = {1.0, NAN, 3.0};
    int result = cxf_check_nan(arr, 3);
    TEST_ASSERT_EQUAL_INT(1, result);  /* NaN at index 1 */
}

void test_check_nan_empty_array(void) {
    double arr[] = {1.0};
    int result = cxf_check_nan(arr, 0);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Empty is clean */
}

void test_check_nan_null_array(void) {
    int result = cxf_check_nan(NULL, 5);
    TEST_ASSERT_EQUAL_INT(-1, result);  /* Error indicator */
}

void test_check_nan_inf_not_detected(void) {
    double arr[] = {1.0, INFINITY, 3.0};
    int result = cxf_check_nan(arr, 3);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Inf is NOT NaN */
}

/*============================================================================
 * cxf_check_nan_or_inf Tests
 *===========================================================================*/

void test_check_nan_or_inf_clean_array(void) {
    double arr[] = {1.0, -2.0, 0.0, DBL_MAX, -DBL_MAX};
    int result = cxf_check_nan_or_inf(arr, 5);
    TEST_ASSERT_EQUAL_INT(0, result);  /* All finite */
}

void test_check_nan_or_inf_with_nan(void) {
    double arr[] = {1.0, 2.0, NAN};
    int result = cxf_check_nan_or_inf(arr, 3);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Found bad value */
}

void test_check_nan_or_inf_with_inf(void) {
    double arr[] = {1.0, INFINITY, 3.0};
    int result = cxf_check_nan_or_inf(arr, 3);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Found infinity */
}

void test_check_nan_or_inf_with_neg_inf(void) {
    double arr[] = {-INFINITY, 2.0, 3.0};
    int result = cxf_check_nan_or_inf(arr, 3);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Found -infinity */
}

void test_check_nan_or_inf_null_array(void) {
    int result = cxf_check_nan_or_inf(NULL, 5);
    TEST_ASSERT_EQUAL_INT(-1, result);  /* Error indicator */
}

/*============================================================================
 * cxf_checkenv Tests
 *===========================================================================*/

void test_checkenv_valid(void) {
    int result = cxf_checkenv(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_checkenv_null(void) {
    int result = cxf_checkenv(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_checkenv_invalid_magic(void) {
    /* Create a fake env with wrong magic - test validates magic check */
    CxfEnv fake_env;
    fake_env.magic = 0xDEADBEEF;  /* Wrong magic */
    int result = cxf_checkenv(&fake_env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * cxf_pivot_check Tests
 *===========================================================================*/

void test_pivot_check_valid(void) {
    int result = cxf_pivot_check(1.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Valid pivot */
}

void test_pivot_check_too_small(void) {
    int result = cxf_pivot_check(1e-12, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Pivot too small */
}

void test_pivot_check_zero(void) {
    int result = cxf_pivot_check(0.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Zero pivot */
}

void test_pivot_check_negative(void) {
    int result = cxf_pivot_check(-1.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Negative but large enough */
}

void test_pivot_check_nan(void) {
    int result = cxf_pivot_check(NAN, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NaN is invalid */
}

/*============================================================================
 * cxf_check_model_flags1 Tests (MIP detection)
 *===========================================================================*/

void test_check_model_flags1_null_model(void) {
    int result = cxf_check_model_flags1(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 (pure continuous) */
}

void test_check_model_flags1_pure_continuous(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Add some continuous variables */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");
    cxf_addvar(model, 0.0, 10.0, 2.0, 'C', "x1");

    int result = cxf_check_model_flags1(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Pure continuous */

    cxf_freemodel(model);
}

void test_check_model_flags1_with_binary(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Add one binary variable */
    cxf_addvar(model, 0.0, 1.0, 1.0, 'B', "b0");

    int result = cxf_check_model_flags1(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has MIP feature */

    cxf_freemodel(model);
}

void test_check_model_flags1_with_integer(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Add one integer variable */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'I', "i0");

    int result = cxf_check_model_flags1(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has MIP feature */

    cxf_freemodel(model);
}

void test_check_model_flags1_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Empty model (no variables) */
    int result = cxf_check_model_flags1(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Empty is pure continuous */

    cxf_freemodel(model);
}

/*============================================================================
 * cxf_check_model_flags2 Tests (Quadratic/conic detection)
 *===========================================================================*/

void test_check_model_flags2_null_model(void) {
    int result = cxf_check_model_flags2(NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 (pure linear) */
}

void test_check_model_flags2_pure_linear(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Add some variables */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");
    cxf_addvar(model, 0.0, 10.0, 2.0, 'C', "x1");

    int result = cxf_check_model_flags2(model, 0);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Pure linear */

    cxf_freemodel(model);
}

void test_check_model_flags2_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model);

    /* Empty model */
    int result = cxf_check_model_flags2(model, 0);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Empty is pure linear */

    cxf_freemodel(model);
}

/*============================================================================
 * cxf_check_terminate Tests
 *===========================================================================*/

void test_check_terminate_null_env(void) {
    int result = cxf_check_terminate(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 (no termination) */
}

void test_check_terminate_not_set(void) {
    /* Fresh env should have no termination requested */
    int result = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_check_terminate_after_terminate(void) {
    cxf_terminate(env);
    int result = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Termination requested */
}

void test_check_terminate_after_clear(void) {
    cxf_terminate(env);
    cxf_reset_terminate(env);
    int result = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Cleared */
}

void test_terminate_null_env_safe(void) {
    cxf_terminate(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_clear_terminate_null_env_safe(void) {
    cxf_reset_terminate(NULL);  /* Should not crash */
    TEST_PASS();
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_error tests */
    RUN_TEST(test_error_basic_message);
    RUN_TEST(test_error_formatted_message);
    RUN_TEST(test_error_null_env_safe);
    RUN_TEST(test_error_empty_message);
    RUN_TEST(test_geterrormsg_null_env);

    /* cxf_errorlog tests */
    RUN_TEST(test_errorlog_null_env);
    RUN_TEST(test_errorlog_null_message);
    RUN_TEST(test_errorlog_basic);

    /* cxf_check_nan tests */
    RUN_TEST(test_check_nan_clean_array);
    RUN_TEST(test_check_nan_with_nan);
    RUN_TEST(test_check_nan_empty_array);
    RUN_TEST(test_check_nan_null_array);
    RUN_TEST(test_check_nan_inf_not_detected);

    /* cxf_check_nan_or_inf tests */
    RUN_TEST(test_check_nan_or_inf_clean_array);
    RUN_TEST(test_check_nan_or_inf_with_nan);
    RUN_TEST(test_check_nan_or_inf_with_inf);
    RUN_TEST(test_check_nan_or_inf_with_neg_inf);
    RUN_TEST(test_check_nan_or_inf_null_array);

    /* cxf_checkenv tests */
    RUN_TEST(test_checkenv_valid);
    RUN_TEST(test_checkenv_null);
    RUN_TEST(test_checkenv_invalid_magic);

    /* cxf_pivot_check tests */
    RUN_TEST(test_pivot_check_valid);
    RUN_TEST(test_pivot_check_too_small);
    RUN_TEST(test_pivot_check_zero);
    RUN_TEST(test_pivot_check_negative);
    RUN_TEST(test_pivot_check_nan);

    /* cxf_check_model_flags1 tests */
    RUN_TEST(test_check_model_flags1_null_model);
    RUN_TEST(test_check_model_flags1_pure_continuous);
    RUN_TEST(test_check_model_flags1_with_binary);
    RUN_TEST(test_check_model_flags1_with_integer);
    RUN_TEST(test_check_model_flags1_empty_model);

    /* cxf_check_model_flags2 tests */
    RUN_TEST(test_check_model_flags2_null_model);
    RUN_TEST(test_check_model_flags2_pure_linear);
    RUN_TEST(test_check_model_flags2_empty_model);

    /* cxf_check_terminate tests */
    RUN_TEST(test_check_terminate_null_env);
    RUN_TEST(test_check_terminate_not_set);
    RUN_TEST(test_check_terminate_after_terminate);
    RUN_TEST(test_check_terminate_after_clear);
    RUN_TEST(test_terminate_null_env_safe);
    RUN_TEST(test_clear_terminate_null_env_safe);

    return UNITY_END();
}
