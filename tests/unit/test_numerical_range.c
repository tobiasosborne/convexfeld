/**
 * @file test_numerical_range.c
 * @brief Numerical stability tests: extreme coefficient ranges and tiny values.
 *
 * Exercises the solver with large coefficient ratios, values near machine
 * epsilon, mixed large/small RHS, and extreme bound spreads.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include <math.h>

int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "range", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env);     env = NULL;
}

/* Coefficients differing by 1e10: one constraint coefficient is 1,
 * another is 1e10. Tests scaling robustness. */
void test_large_coefficient_ratio(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1e10, 1.0};
    double v2[] = {1.0, 1e10};
    cxf_addconstr(model, 2, ind, v1, '<', 1e10, "big_x");
    cxf_addconstr(model, 2, ind, v2, '<', 1e10, "big_y");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Large coeff ratio should yield OPTIMAL or NUMERIC");

    if (status == CXF_OPTIMAL) {
        double objval;
        cxf_getdblattr(model, "ObjVal", &objval);
        /* Feasible region allows x ~ 1, y ~ 1 at corner, obj ~ -2 */
        TEST_ASSERT_TRUE_MESSAGE(isfinite(objval),
            "Objective must be finite");
    }
}

/* Coefficients near machine epsilon (1e-15). The solver should not
 * treat these as zero and produce garbage. */
void test_near_epsilon_coefficients(void) {
    cxf_addvar(model, 0, NULL, NULL, 1e-15, 0.0, 1e15, 'C', "x");

    int ind[] = {0};
    double val[] = {1e-15};
    cxf_addconstr(model, 1, ind, val, '<', 1.0, "tiny");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_UNBOUNDED || status == CXF_INF_OR_UNBD,
        "Near-epsilon coefficients should not crash");
}

/* Mixed large and small RHS values in the same problem. */
void test_mixed_rhs_magnitudes(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 0.0};
    double v2[] = {0.0, 1.0};
    cxf_addconstr(model, 2, ind, v1, '<', 1e8, "big_rhs");
    cxf_addconstr(model, 2, ind, v2, '<', 1e-4, "small_rhs");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Mixed RHS should solve to OPTIMAL");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    /* x <= 1e8, y <= 1e-4 => obj = -(1e8 + 1e-4) ~ -1e8 */
    TEST_ASSERT_DOUBLE_WITHIN(1.0, -1e8, objval);
}

/* Very large bounds with unit coefficients. */
void test_extreme_bounds(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e15, 'C', "x");

    int ind[] = {0};
    double val[] = {1.0};
    cxf_addconstr(model, 1, ind, val, '<', 1e12, "cap");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Extreme bounds should still solve");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, -1e12, objval);
}

/* Constraint with coefficients of alternating very large/small magnitude. */
void test_alternating_magnitudes(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1e-8, 0.0, CXF_INFINITY, 'C', "y");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "z");

    int ind[] = {0, 1, 2};
    double v1[] = {1e6, 1e-6, 1e6};
    double v2[] = {1e-6, 1e6, 1e-6};
    cxf_addconstr(model, 3, ind, v1, '<', 1e6, "alt1");
    cxf_addconstr(model, 3, ind, v2, '<', 1e6, "alt2");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Alternating magnitudes should not crash");
}

/* Objective with very large positive coefficients (minimization pushes
 * everything to lower bound). Trivial but tests no overflow. */
void test_large_objective_coefficients(void) {
    cxf_addvar(model, 0, NULL, NULL, 1e12, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, 1e12, 0.0, 100.0, 'C', "y");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Large obj coeffs should push vars to lower bound");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    /* Both vars at lb=0 => obj = 0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, objval);
}

/* RHS = 0 with equality constraint (homogeneous system).
 * Only solution is x=y=0. */
void test_zero_rhs_equality(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    cxf_addconstr(model, 2, ind, val, '=', 0.0, "zero_rhs");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Zero-RHS equality should be feasible at origin");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, objval);
}

/* Very tight bounds: lb and ub differ by 1e-10. Variable is nearly fixed. */
void test_nearly_fixed_variable(void) {
    double lb = 5.0;
    double ub = 5.0 + 1e-10;
    cxf_addvar(model, 0, NULL, NULL, 1.0, lb, ub, 'C', "tight");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Nearly fixed variable should solve");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, objval);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_large_coefficient_ratio);
    RUN_TEST(test_near_epsilon_coefficients);
    RUN_TEST(test_mixed_rhs_magnitudes);
    RUN_TEST(test_extreme_bounds);
    RUN_TEST(test_alternating_magnitudes);
    RUN_TEST(test_large_objective_coefficients);
    RUN_TEST(test_zero_rhs_equality);
    RUN_TEST(test_nearly_fixed_variable);
    return UNITY_END();
}
