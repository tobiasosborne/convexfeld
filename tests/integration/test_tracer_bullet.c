/**
 * @file test_tracer_bullet.c
 * @brief Tracer Bullet Test - End-to-end integration test for 1-variable LP.
 *
 * This test proves the end-to-end architecture works by solving a trivial LP:
 *   min x subject to x >= 0
 *
 * Expected: x* = 0, obj* = 0
 *
 * Functions tested:
 *   - cxf_loadenv
 *   - cxf_newmodel
 *   - cxf_addvar
 *   - cxf_optimize
 *   - cxf_getintattr
 *   - cxf_getdblattr
 *   - cxf_freemodel
 *   - cxf_freeenv
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"

void setUp(void) {
    /* No per-test setup needed */
}

void tearDown(void) {
    /* No per-test cleanup needed */
}

/**
 * @brief Test solving a trivial 1-variable LP.
 *
 * Problem:
 *   minimize   x
 *   subject to x >= 0
 *
 * The optimal solution is x* = 0 with objective value 0.
 */
void test_tracer_bullet_1var_lp(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;
    int opt_status;
    double objval;

    /* Create environment */
    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_loadenv failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(env, "env should not be NULL after loadenv");

    /* Create model */
    status = cxf_newmodel(env, &model, "tracer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_newmodel failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(model, "model should not be NULL after newmodel");

    /* Add variable: min x, lb=0, ub=inf, obj=1.0, type='C', name="x" */
    status = cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addvar failed");

    /* Optimize */
    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_optimize failed");

    /* Check optimization status */
    status = cxf_getintattr(model, "Status", &opt_status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getintattr(Status) failed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, opt_status,
                                  "Expected OPTIMAL status");

    /* Check objective value */
    status = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getdblattr(ObjVal) failed");
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, 0.0, objval,
                                      "Expected objective value 0.0");

    /* Cleanup */
    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tracer_bullet_1var_lp);
    return UNITY_END();
}
