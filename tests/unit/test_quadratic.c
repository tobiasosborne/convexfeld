/**
 * @file test_quadratic.c
 * @brief Unit tests for cxf_quadratic_adjust function.
 */

#include <stdio.h>
#include <math.h>
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "unity.h"

/* Test fixture state */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;
static SolverContext *state = NULL;

void setUp(void) {
    /* Create environment */
    int ret = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
    TEST_ASSERT_NOT_NULL(env);

    /* Create model */
    ret = cxf_newmodel(env, &model, "test_model", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
    TEST_ASSERT_NOT_NULL(model);

    /* Add two variables */
    ret = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, CXF_CONTINUOUS, "x0");
    TEST_ASSERT_EQUAL(CXF_OK, ret);
    ret = cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, CXF_CONTINUOUS, "x1");
    TEST_ASSERT_EQUAL(CXF_OK, ret);

    /* Initialize solver context */
    ret = cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) {
    if (state != NULL) {
        cxf_simplex_final(state);
        state = NULL;
    }
    if (model != NULL) {
        cxf_freemodel(model);
        model = NULL;
    }
    if (env != NULL) {
        cxf_freeenv(env);
        env = NULL;
    }
}

/**
 * @brief Test that cxf_quadratic_adjust returns error for NULL state.
 */
void test_quadratic_null_state(void) {
    int ret = cxf_quadratic_adjust(NULL, -1);
    TEST_ASSERT_EQUAL(CXF_ERROR_NULL_ARGUMENT, ret);
}

/**
 * @brief Test that cxf_quadratic_adjust succeeds with valid state (all variables).
 */
void test_quadratic_all_variables(void) {
    int ret = cxf_quadratic_adjust(state, -1);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
}

/**
 * @brief Test that cxf_quadratic_adjust succeeds with single variable.
 */
void test_quadratic_single_variable(void) {
    int ret = cxf_quadratic_adjust(state, 0);
    TEST_ASSERT_EQUAL(CXF_OK, ret);

    ret = cxf_quadratic_adjust(state, 1);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
}

/**
 * @brief Test that cxf_quadratic_adjust returns error for invalid varIndex.
 */
void test_quadratic_invalid_index(void) {
    /* Index out of range */
    int ret = cxf_quadratic_adjust(state, 2);
    TEST_ASSERT_EQUAL(CXF_ERROR_INVALID_ARGUMENT, ret);

    /* Invalid negative index (not -1) */
    ret = cxf_quadratic_adjust(state, -2);
    TEST_ASSERT_EQUAL(CXF_ERROR_INVALID_ARGUMENT, ret);
}

/**
 * @brief Test that multiple calls to cxf_quadratic_adjust are safe.
 */
void test_quadratic_multiple_calls(void) {
    /* Call multiple times should succeed (stub always returns OK) */
    int ret = cxf_quadratic_adjust(state, -1);
    TEST_ASSERT_EQUAL(CXF_OK, ret);

    ret = cxf_quadratic_adjust(state, 0);
    TEST_ASSERT_EQUAL(CXF_OK, ret);

    ret = cxf_quadratic_adjust(state, -1);
    TEST_ASSERT_EQUAL(CXF_OK, ret);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_quadratic_null_state);
    RUN_TEST(test_quadratic_all_variables);
    RUN_TEST(test_quadratic_single_variable);
    RUN_TEST(test_quadratic_invalid_index);
    RUN_TEST(test_quadratic_multiple_calls);
    return UNITY_END();
}
