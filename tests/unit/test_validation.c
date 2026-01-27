/**
 * @file test_validation.c
 * @brief TDD tests for validation functions (M2.3.1, M2.3.2)
 *
 * Tests for validation module functions:
 * - cxf_validate_array - validate array for NaN values
 * - cxf_validate_vartypes - validate variable type characters
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for validation functions */
int cxf_validate_array(CxfEnv *env, int count, const double *array);
int cxf_validate_vartypes(CxfModel *model);

/* Helper to create a minimal test model */
static CxfModel *create_test_model(int num_vars) {
    CxfModel *model = (CxfModel *)calloc(1, sizeof(CxfModel));
    if (model == NULL) return NULL;

    model->num_vars = num_vars;
    if (num_vars > 0) {
        model->lb = (double *)malloc((size_t)num_vars * sizeof(double));
        model->ub = (double *)malloc((size_t)num_vars * sizeof(double));
        model->vtype = (char *)malloc((size_t)num_vars * sizeof(char));

        if (model->lb == NULL || model->ub == NULL || model->vtype == NULL) {
            free(model->lb);
            free(model->ub);
            free(model->vtype);
            free(model);
            return NULL;
        }

        /* Default to continuous variables with [0, infinity) */
        for (int i = 0; i < num_vars; i++) {
            model->lb[i] = 0.0;
            model->ub[i] = INFINITY;
            model->vtype[i] = 'C';
        }
    }
    return model;
}

static void free_test_model(CxfModel *model) {
    if (model) {
        free(model->lb);
        free(model->ub);
        free(model->vtype);
        free(model);
    }
}

/* Test fixtures */
void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * cxf_validate_array Tests
 *===========================================================================*/

void test_cxf_validate_array_valid(void) {
    double arr[] = {1.0, 2.5, -3.7, 0.0, 100.0};
    int result = cxf_validate_array(NULL, 5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_null_array(void) {
    int result = cxf_validate_array(NULL, 10, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_zero_count(void) {
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, 0, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_negative_count(void) {
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, -5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_nan(void) {
    double arr[] = {1.0, NAN, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_first(void) {
    double arr[] = {NAN, 1.0, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_last(void) {
    double arr[] = {1.0, 2.0, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_all_nan(void) {
    double arr[] = {NAN, NAN, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_inf(void) {
    double arr[] = {1.0, INFINITY, -INFINITY};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_element(void) {
    double arr[] = {42.0};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_nan(void) {
    double arr[] = {NAN};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * cxf_validate_vartypes Tests
 *===========================================================================*/

void test_cxf_validate_vartypes_null_model(void) {
    int result = cxf_validate_vartypes(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_vartypes_all_continuous(void) {
    CxfModel *model = create_test_model(3);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'C';
    model->vtype[1] = 'C';
    model->vtype[2] = 'C';

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_all_valid_types(void) {
    CxfModel *model = create_test_model(5);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'C';  /* Continuous */
    model->vtype[1] = 'B';  /* Binary */
    model->vtype[2] = 'I';  /* Integer */
    model->vtype[3] = 'S';  /* Semi-continuous */
    model->vtype[4] = 'N';  /* Semi-integer */

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_invalid_type(void) {
    CxfModel *model = create_test_model(3);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'C';
    model->vtype[1] = 'X';  /* Invalid */
    model->vtype[2] = 'C';

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_lowercase_invalid(void) {
    CxfModel *model = create_test_model(2);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'c';  /* lowercase - invalid */
    model->vtype[1] = 'B';

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_null_vtype(void) {
    CxfModel *model = create_test_model(3);
    TEST_ASSERT_NOT_NULL(model);

    free(model->vtype);
    model->vtype = NULL;  /* NULL means all continuous */

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_binary_clamps_lb(void) {
    CxfModel *model = create_test_model(1);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'B';
    model->lb[0] = -5.0;  /* Out of range */
    model->ub[0] = 1.0;

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[0]);  /* Clamped to 0 */

    free_test_model(model);
}

void test_cxf_validate_vartypes_binary_clamps_ub(void) {
    CxfModel *model = create_test_model(1);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'B';
    model->lb[0] = 0.0;
    model->ub[0] = 10.0;  /* Out of range */

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->ub[0]);  /* Clamped to 1 */

    free_test_model(model);
}

void test_cxf_validate_vartypes_binary_valid_bounds(void) {
    CxfModel *model = create_test_model(1);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'B';
    model->lb[0] = 0.0;
    model->ub[0] = 1.0;

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->ub[0]);

    free_test_model(model);
}

void test_cxf_validate_vartypes_binary_infeasible(void) {
    CxfModel *model = create_test_model(1);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'B';
    model->lb[0] = 2.0;   /* Clamps to 1 */
    model->ub[0] = 0.5;   /* Clamps to 0.5 */
    /* After clamping: lb=1 > ub=0.5 - infeasible */

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_zero_vars(void) {
    CxfModel *model = create_test_model(0);
    TEST_ASSERT_NOT_NULL(model);

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    free_test_model(model);
}

void test_cxf_validate_vartypes_mixed_with_binary_clamp(void) {
    CxfModel *model = create_test_model(3);
    TEST_ASSERT_NOT_NULL(model);

    model->vtype[0] = 'C';
    model->lb[0] = -100.0;
    model->ub[0] = 100.0;

    model->vtype[1] = 'B';
    model->lb[1] = -1.0;  /* Should clamp to 0 */
    model->ub[1] = 2.0;   /* Should clamp to 1 */

    model->vtype[2] = 'I';
    model->lb[2] = 0.0;
    model->ub[2] = 10.0;

    int result = cxf_validate_vartypes(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    /* Continuous unchanged */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -100.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 100.0, model->ub[0]);

    /* Binary clamped */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->ub[1]);

    /* Integer unchanged */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, model->ub[2]);

    free_test_model(model);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_validate_array tests */
    RUN_TEST(test_cxf_validate_array_valid);
    RUN_TEST(test_cxf_validate_array_null_array);
    RUN_TEST(test_cxf_validate_array_zero_count);
    RUN_TEST(test_cxf_validate_array_negative_count);
    RUN_TEST(test_cxf_validate_array_nan);
    RUN_TEST(test_cxf_validate_array_nan_first);
    RUN_TEST(test_cxf_validate_array_nan_last);
    RUN_TEST(test_cxf_validate_array_all_nan);
    RUN_TEST(test_cxf_validate_array_inf);
    RUN_TEST(test_cxf_validate_array_single_element);
    RUN_TEST(test_cxf_validate_array_single_nan);

    /* cxf_validate_vartypes tests */
    RUN_TEST(test_cxf_validate_vartypes_null_model);
    RUN_TEST(test_cxf_validate_vartypes_all_continuous);
    RUN_TEST(test_cxf_validate_vartypes_all_valid_types);
    RUN_TEST(test_cxf_validate_vartypes_invalid_type);
    RUN_TEST(test_cxf_validate_vartypes_lowercase_invalid);
    RUN_TEST(test_cxf_validate_vartypes_null_vtype);
    RUN_TEST(test_cxf_validate_vartypes_binary_clamps_lb);
    RUN_TEST(test_cxf_validate_vartypes_binary_clamps_ub);
    RUN_TEST(test_cxf_validate_vartypes_binary_valid_bounds);
    RUN_TEST(test_cxf_validate_vartypes_binary_infeasible);
    RUN_TEST(test_cxf_validate_vartypes_zero_vars);
    RUN_TEST(test_cxf_validate_vartypes_mixed_with_binary_clamp);

    return UNITY_END();
}
