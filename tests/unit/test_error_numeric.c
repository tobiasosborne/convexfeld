/**
 * @file test_error_numeric.c
 * @brief Tests for numeric validation and model flag checks:
 *        cxf_check_nan, cxf_is_finite, cxf_check_model_flags1,
 *        cxf_check_model_flags2 (M3.1.1)
 *
 * Split from test_error.c. Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include <math.h>
#include <float.h>

/* Forward declarations for functions under test */
int cxf_check_nan(const double *arr, int n);
int cxf_is_finite(const double *arr, int n);
int cxf_check_model_flags1(CxfModel *model);
int cxf_check_model_flags2(CxfModel *model, int flag);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/*============================================================================
 * cxf_check_nan Tests
 *===========================================================================*/

void test_check_nan_clean_array(void) {
    double arr[] = {1.0, 2.0, 3.0, -4.5, 0.0};
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(arr, 5));
}

void test_check_nan_with_nan(void) {
    double arr[] = {1.0, NAN, 3.0};
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan(arr, 3));
}

void test_check_nan_empty_array(void) {
    double arr[] = {1.0};
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(arr, 0));
}

void test_check_nan_null_array(void) {
    TEST_ASSERT_EQUAL_INT(-1, cxf_check_nan(NULL, 5));
}

void test_check_nan_inf_not_detected(void) {
    double arr[] = {1.0, INFINITY, 3.0};
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(arr, 3));  /* Inf is NOT NaN */
}

/*============================================================================
 * cxf_is_finite Tests
 *===========================================================================*/

void test_check_nan_or_inf_clean_array(void) {
    double arr[] = {1.0, -2.0, 0.0, DBL_MAX, -DBL_MAX};
    TEST_ASSERT_EQUAL_INT(0, cxf_is_finite(arr, 5));
}

void test_check_nan_or_inf_with_nan(void) {
    double arr[] = {1.0, 2.0, NAN};
    TEST_ASSERT_EQUAL_INT(1, cxf_is_finite(arr, 3));
}

void test_check_nan_or_inf_with_inf(void) {
    double arr[] = {1.0, INFINITY, 3.0};
    TEST_ASSERT_EQUAL_INT(1, cxf_is_finite(arr, 3));
}

void test_check_nan_or_inf_with_neg_inf(void) {
    double arr[] = {-INFINITY, 2.0, 3.0};
    TEST_ASSERT_EQUAL_INT(1, cxf_is_finite(arr, 3));
}

void test_check_nan_or_inf_null_array(void) {
    TEST_ASSERT_EQUAL_INT(-1, cxf_is_finite(NULL, 5));
}

/*============================================================================
 * cxf_check_model_flags1 Tests (MIP detection)
 *===========================================================================*/

void test_check_model_flags1_null_model(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(NULL));
}

void test_check_model_flags1_pure_continuous(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(model));
    cxf_freemodel(model);
}

void test_check_model_flags1_with_binary(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'B', "b0");
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(model));
    cxf_freemodel(model);
}

void test_check_model_flags1_with_integer(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'I', "i0");
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(model));
    cxf_freemodel(model);
}

void test_check_model_flags1_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(model));
    cxf_freemodel(model);
}

/*============================================================================
 * cxf_check_model_flags2 Tests (Quadratic/conic detection)
 *===========================================================================*/

void test_check_model_flags2_null_model(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(NULL, 0));
}

void test_check_model_flags2_pure_linear(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(model, 0));
    cxf_freemodel(model);
}

void test_check_model_flags2_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(model, 0));
    cxf_freemodel(model);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_check_nan_clean_array);
    RUN_TEST(test_check_nan_with_nan);
    RUN_TEST(test_check_nan_empty_array);
    RUN_TEST(test_check_nan_null_array);
    RUN_TEST(test_check_nan_inf_not_detected);

    RUN_TEST(test_check_nan_or_inf_clean_array);
    RUN_TEST(test_check_nan_or_inf_with_nan);
    RUN_TEST(test_check_nan_or_inf_with_inf);
    RUN_TEST(test_check_nan_or_inf_with_neg_inf);
    RUN_TEST(test_check_nan_or_inf_null_array);

    RUN_TEST(test_check_model_flags1_null_model);
    RUN_TEST(test_check_model_flags1_pure_continuous);
    RUN_TEST(test_check_model_flags1_with_binary);
    RUN_TEST(test_check_model_flags1_with_integer);
    RUN_TEST(test_check_model_flags1_empty_model);

    RUN_TEST(test_check_model_flags2_null_model);
    RUN_TEST(test_check_model_flags2_pure_linear);
    RUN_TEST(test_check_model_flags2_empty_model);

    return UNITY_END();
}
