/**
 * @file test_status_clear.c
 * @brief Tests that cxf_optimize clears model status before solving.
 *
 * Per solve_entry.md Step 4, cxf_optimize must clear the model's status
 * code so stale status from a previous solve does not leak into the next.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Test fixture */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/**
 * Test: stale INFEASIBLE status is cleared on re-optimize.
 *
 * Sets model->status to CXF_INFEASIBLE (simulating a previous solve),
 * then calls cxf_optimize. The status must not remain INFEASIBLE.
 */
void test_status_cleared_before_solve(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "clear_test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    /* Inject stale status from a "previous solve" */
    model->status = CXF_INFEASIBLE;

    /* Optimize should clear the stale status */
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Status must NOT be the stale INFEASIBLE value */
    TEST_ASSERT_NOT_EQUAL(CXF_INFEASIBLE, model->status);

    cxf_freemodel(model);
}

/**
 * Test: stale NUMERIC status is cleared on re-optimize.
 *
 * Same pattern with a different stale status value.
 */
void test_numeric_status_cleared(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "numeric_clear", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 5.0, 'C', "y");

    /* Inject stale NUMERIC status */
    model->status = CXF_NUMERIC;

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Must not remain NUMERIC after a successful solve */
    TEST_ASSERT_NOT_EQUAL(CXF_NUMERIC, model->status);

    cxf_freemodel(model);
}

/**
 * Test: stale ITERATION_LIMIT status is cleared on re-optimize.
 */
void test_iter_limit_status_cleared(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "iter_clear", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 8.0, 'C', "z");

    /* Inject stale ITERATION_LIMIT status */
    model->status = CXF_ITERATION_LIMIT;

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Must not remain ITERATION_LIMIT after a successful solve */
    TEST_ASSERT_NOT_EQUAL(CXF_ITERATION_LIMIT, model->status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_status_cleared_before_solve);
    RUN_TEST(test_numeric_status_cleared);
    RUN_TEST(test_iter_limit_status_cleared);

    return UNITY_END();
}
