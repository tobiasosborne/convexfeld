/**
 * @file test_constrained_lp.c
 * @brief Integration test for constrained LP solving.
 *
 * Verifies the simplex solver correctly handles constrained LPs.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"

/* External declaration for constraint API */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test_lp", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/**
 * @brief Test solving a constrained 2-variable LP.
 *
 * Problem:
 *   minimize   -x - y
 *   subject to x + y <= 4
 *              x <= 2
 *              y <= 3
 *              x, y >= 0
 *
 * Optimal: x=1, y=3 with objective value -4
 * (or x=2, y=2 is also optimal, both give obj=-4)
 */
void test_constrained_lp_2var(void) {
    int status;
    int opt_status;
    double objval;

    /* Add variables: x and y with obj coeffs -1, bounds [0, inf] */
    status = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addvar(x) failed");

    status = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addvar(y) failed");

    /* Add constraint: x + y <= 4 */
    int ind1[] = {0, 1};
    double val1[] = {1.0, 1.0};
    status = cxf_addconstr(model, 2, ind1, val1, '<', 4.0, "sum");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addconstr(sum) failed");

    /* Add constraint: x <= 2 */
    int ind2[] = {0};
    double val2[] = {1.0};
    status = cxf_addconstr(model, 1, ind2, val2, '<', 2.0, "x_bound");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addconstr(x_bound) failed");

    /* Add constraint: y <= 3 */
    int ind3[] = {1};
    double val3[] = {1.0};
    status = cxf_addconstr(model, 1, ind3, val3, '<', 3.0, "y_bound");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addconstr(y_bound) failed");

    /* Optimize */
    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_optimize failed");

    /* Check optimization status */
    status = cxf_getintattr(model, "Status", &opt_status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getintattr(Status) failed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, opt_status, "Expected OPTIMAL status");

    /* Check objective value: should be -4 */
    status = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getdblattr(ObjVal) failed");
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, -4.0, objval,
                                      "Expected objective value -4.0");
}

/**
 * @brief Test a single-constraint LP.
 *
 * Problem:
 *   minimize   -x
 *   subject to x <= 5
 *              x >= 0
 *
 * Optimal: x=5 with objective value -5
 */
void test_single_constraint_lp(void) {
    int status;
    int opt_status;
    double objval;

    /* Add variable x with obj coeff -1, bounds [0, inf] */
    status = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addvar failed");

    /* Add constraint: x <= 5 */
    int ind[] = {0};
    double val[] = {1.0};
    status = cxf_addconstr(model, 1, ind, val, '<', 5.0, "c1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_addconstr failed");

    /* Optimize */
    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_optimize failed");

    /* Check optimization status */
    status = cxf_getintattr(model, "Status", &opt_status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getintattr(Status) failed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, opt_status, "Expected OPTIMAL status");

    /* Check objective value: should be -5 */
    status = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, status, "cxf_getdblattr(ObjVal) failed");
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, -5.0, objval,
                                      "Expected objective value -5.0");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_constrained_lp_2var);
    RUN_TEST(test_single_constraint_lp);
    return UNITY_END();
}
