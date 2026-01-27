/**
 * @file test_api_vars.c
 * @brief TDD tests for variable API functions.
 *
 * M8.1.3: API Tests - Variables
 *
 * Tests: cxf_addvar(extended), 0, NULL, NULL, int numvars, cxf_addvars, cxf_delvars
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations for functions under test */
int cxf_addvars(CxfModel *model, int numvars, int numnz,
                const int *vbeg, const int *vind, const double *vval,
                const double *obj, const double *lb, const double *ub,
                const char *vtype, const char **varnames);
int cxf_delvars(CxfModel *model, int numdel, const int *ind);

/* Test fixture - shared environment */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * cxf_addvar Extended Tests - Variable Types
 ******************************************************************************/

void test_addvar_binary_variable(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'B', "binary");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_vars);

    cxf_freemodel(model);
}

void test_addvar_integer_variable(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 100.0, 'I', "integer");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_addvar_unbounded_variable(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Unbounded below and above */
    int status = cxf_addvar(model, 0, NULL, NULL, 1.0, -CXF_INFINITY, CXF_INFINITY, 'C', "free");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -CXF_INFINITY, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, CXF_INFINITY, model->ub[0]);

    cxf_freemodel(model);
}

void test_addvar_negative_bounds(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvar(model, 0, NULL, NULL, 1.0, -10.0, -1.0, 'C', "negative");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -10.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, model->ub[0]);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_addvars Tests - Batch Variable Addition
 ******************************************************************************/

void test_addvars_basic_batch(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    double obj[] = {1.0, 2.0, 3.0};
    double lb[] = {0.0, 0.0, 0.0};
    double ub[] = {10.0, 20.0, 30.0};

    int status = cxf_addvars(model, 3, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(3, model->num_vars);

    cxf_freemodel(model);
}

void test_addvars_null_model_returns_error(void) {
    double obj[] = {1.0};
    int status = cxf_addvars(NULL, 1, 0, NULL, NULL, NULL, obj, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_addvars_zero_vars_succeeds(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvars(model, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, model->num_vars);

    cxf_freemodel(model);
}

void test_addvars_stores_correct_values(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    double obj[] = {1.5, 2.5};
    double lb[] = {5.0, 10.0};
    double ub[] = {15.0, 25.0};

    cxf_addvars(model, 2, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.5, model->obj_coeffs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, model->obj_coeffs[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, model->lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 15.0, model->ub[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 25.0, model->ub[1]);

    cxf_freemodel(model);
}

void test_addvars_null_arrays_use_defaults(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* NULL obj/lb/ub should use defaults: 0, 0, infinity */
    int status = cxf_addvars(model, 2, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, model->num_vars);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->obj_coeffs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, CXF_INFINITY, model->ub[0]);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Dynamic Capacity Tests - Array Resizing
 ******************************************************************************/

void test_addvar_exceeds_initial_capacity(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Initial capacity is 16, add 20 variables to trigger growth */
    for (int i = 0; i < 20; i++) {
        int status = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', NULL);
        TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    }

    TEST_ASSERT_EQUAL_INT(20, model->num_vars);
    TEST_ASSERT_GREATER_OR_EQUAL(20, model->var_capacity);

    /* Verify all variables have correct values */
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[i]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, model->ub[i]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->obj_coeffs[i]);
    }

    cxf_freemodel(model);
}

void test_addvars_batch_exceeds_capacity(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Add 50 variables at once (exceeds initial capacity of 16) */
    int status = cxf_addvars(model, 50, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(50, model->num_vars);
    TEST_ASSERT_GREATER_OR_EQUAL(50, model->var_capacity);

    /* Verify defaults are applied correctly */
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->obj_coeffs[i]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->lb[i]);
        TEST_ASSERT_DOUBLE_WITHIN(1e-10, CXF_INFINITY, model->ub[i]);
    }

    cxf_freemodel(model);
}

void test_addvar_grows_capacity(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Check initial capacity (should be 16) */
    int initial_capacity = model->var_capacity;
    TEST_ASSERT_EQUAL_INT(16, initial_capacity);

    /* Add enough variables to trigger at least one growth */
    for (int i = 0; i < initial_capacity + 1; i++) {
        cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', NULL);
    }

    /* Verify capacity has grown */
    TEST_ASSERT_GREATER_THAN(initial_capacity, model->var_capacity);
    TEST_ASSERT_EQUAL_INT(initial_capacity + 1, model->num_vars);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_delvars Tests - Variable Deletion
 ******************************************************************************/

void test_delvars_basic(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Add 3 variables */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 2.0, 'C', "x2");
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 3.0, 'C', "x3");
    TEST_ASSERT_EQUAL_INT(3, model->num_vars);

    /* Delete variable 1 */
    int ind[] = {1};
    int status = cxf_delvars(model, 1, ind);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_delvars_null_model_returns_error(void) {
    int ind[] = {0};
    int status = cxf_delvars(NULL, 1, ind);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_delvars_zero_count_succeeds(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x");

    int status = cxf_delvars(model, 0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_vars);  /* Unchanged */

    cxf_freemodel(model);
}

void test_delvars_null_ind_with_nonzero_count_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x");

    int status = cxf_delvars(model, 1, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_delvars_invalid_index_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x");

    int ind[] = {5};  /* Out of range */
    int status = cxf_delvars(model, 1, ind);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_delvars_multiple_vars(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Add 5 variables */
    for (int i = 0; i < 5; i++) {
        cxf_addvar(model, 0, NULL, NULL, (double)(i + 1), 0.0, (double)(i + 1), 'C', NULL);
    }
    TEST_ASSERT_EQUAL_INT(5, model->num_vars);

    /* Delete variables 0, 2, 4 */
    int ind[] = {0, 2, 4};
    int status = cxf_delvars(model, 3, ind);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_addvar extended tests */
    RUN_TEST(test_addvar_binary_variable);
    RUN_TEST(test_addvar_integer_variable);
    RUN_TEST(test_addvar_unbounded_variable);
    RUN_TEST(test_addvar_negative_bounds);

    /* cxf_addvars tests */
    RUN_TEST(test_addvars_basic_batch);
    RUN_TEST(test_addvars_null_model_returns_error);
    RUN_TEST(test_addvars_zero_vars_succeeds);
    RUN_TEST(test_addvars_stores_correct_values);
    RUN_TEST(test_addvars_null_arrays_use_defaults);

    /* Dynamic capacity tests */
    RUN_TEST(test_addvar_exceeds_initial_capacity);
    RUN_TEST(test_addvars_batch_exceeds_capacity);
    RUN_TEST(test_addvar_grows_capacity);

    /* cxf_delvars tests */
    RUN_TEST(test_delvars_basic);
    RUN_TEST(test_delvars_null_model_returns_error);
    RUN_TEST(test_delvars_zero_count_succeeds);
    RUN_TEST(test_delvars_null_ind_with_nonzero_count_fails);
    RUN_TEST(test_delvars_invalid_index_fails);
    RUN_TEST(test_delvars_multiple_vars);

    return UNITY_END();
}
