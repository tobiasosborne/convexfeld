/**
 * @file test_api_optimize.c
 * @brief TDD tests for optimization API functions.
 *
 * M8.1.5: API Tests - Optimize
 *
 * Tests: cxf_optimize, cxf_optimize_internal, cxf_terminate
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations for functions under test */
/* Note: cxf_terminate is declared in cxf_env.h */
int cxf_check_terminate(CxfEnv *env);

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
 * cxf_optimize Tests - Basic Optimization
 ******************************************************************************/

void test_optimize_null_model_fails(void) {
    int status = cxf_optimize(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_optimize_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    /* Empty model (no variables) should still succeed */
    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_optimize_single_variable(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_optimize_multiple_variables(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x1");
    cxf_addvar(model, 0.0, 20.0, 2.0, 'C', "x2");
    cxf_addvar(model, 0.0, 30.0, 3.0, 'C', "x3");

    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_optimize_with_constraints(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    /* Add variables */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x1");
    cxf_addvar(model, 0.0, 10.0, 2.0, 'C', "x2");

    /* Add constraint: x1 + x2 <= 15 */
    int cind[] = {0, 1};
    double cval[] = {1.0, 1.0};
    int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                      const double *cval, char sense, double rhs,
                      const char *constrname);
    cxf_addconstr(model, 2, cind, cval, '<', 15.0, "c1");

    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_terminate Tests - Termination Control
 ******************************************************************************/

void test_terminate_flag_initially_false(void) {
    int terminated = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, terminated);
}

void test_terminate_sets_flag(void) {
    cxf_terminate(env);
    int terminated = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, terminated);
}

void test_clear_terminate_resets_flag(void) {
    cxf_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, cxf_check_terminate(env));

    cxf_reset_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, cxf_check_terminate(env));
}

void test_terminate_null_env_safe(void) {
    /* Should not crash */
    cxf_terminate(NULL);
    cxf_reset_terminate(NULL);
    int result = cxf_check_terminate(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*******************************************************************************
 * Status and Attribute Tests After Optimization
 ******************************************************************************/

void test_status_after_optimize(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    cxf_optimize(model);

    int status;
    cxf_getintattr(model, "Status", &status);
    /* Status should be one of the valid optimization statuses */
    TEST_ASSERT_TRUE(status >= CXF_OK && status <= CXF_NUMERIC);

    cxf_freemodel(model);
}

void test_objval_available_after_optimize(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    cxf_optimize(model);

    double objval;
    int result = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_optimize tests */
    RUN_TEST(test_optimize_null_model_fails);
    RUN_TEST(test_optimize_empty_model);
    RUN_TEST(test_optimize_single_variable);
    RUN_TEST(test_optimize_multiple_variables);
    RUN_TEST(test_optimize_with_constraints);

    /* cxf_terminate tests */
    RUN_TEST(test_terminate_flag_initially_false);
    RUN_TEST(test_terminate_sets_flag);
    RUN_TEST(test_clear_terminate_resets_flag);
    RUN_TEST(test_terminate_null_env_safe);

    /* Status/attribute tests */
    RUN_TEST(test_status_after_optimize);
    RUN_TEST(test_objval_available_after_optimize);

    return UNITY_END();
}
