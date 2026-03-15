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
int cxf_check_model_flags2(CxfModel *model);

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
 * cxf_check_model_flags1 Tests (Active optimization state — V2 spec)
 *===========================================================================*/

void test_check_model_flags1_null_model(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(NULL));
}

void test_check_model_flags1_no_active_state(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Fresh model: self_ptr=NULL, modification_blocked=0 */
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(model));
    cxf_freemodel(model);
}

void test_check_model_flags1_concurrent_solve(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Simulate concurrent solve: self_ptr set */
    model->self_ptr = model;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(model));
    model->self_ptr = NULL;
    cxf_freemodel(model);
}

void test_check_model_flags1_active_with_basis(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Simulate active flag + basis factorization */
    model->modification_blocked = 1;
    int dummy = 42;
    model->solution_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(model));
    model->modification_blocked = 0;
    model->solution_data = NULL;
    cxf_freemodel(model);
}

void test_check_model_flags1_active_no_basis(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Active flag but no basis factorization */
    model->modification_blocked = 1;
    model->solution_data = NULL;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(model));
    model->modification_blocked = 0;
    cxf_freemodel(model);
}

/*============================================================================
 * cxf_check_model_flags2 Tests (Dual data availability — V2 spec)
 *===========================================================================*/

void test_check_model_flags2_null_model(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(NULL));
}

void test_check_model_flags2_no_solve(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Fresh model: no solve indicator */
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(model));
    cxf_freemodel(model);
}

void test_check_model_flags2_no_dual_data(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    /* Solve indicator set but no dual data */
    model->self_ptr = model;
    model->status = CXF_OPTIMAL;
    model->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(model));
    model->self_ptr = NULL;
    model->modification_blocked = 0;
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
    RUN_TEST(test_check_model_flags1_no_active_state);
    RUN_TEST(test_check_model_flags1_concurrent_solve);
    RUN_TEST(test_check_model_flags1_active_with_basis);
    RUN_TEST(test_check_model_flags1_active_no_basis);

    RUN_TEST(test_check_model_flags2_null_model);
    RUN_TEST(test_check_model_flags2_no_solve);
    RUN_TEST(test_check_model_flags2_no_dual_data);

    return UNITY_END();
}
