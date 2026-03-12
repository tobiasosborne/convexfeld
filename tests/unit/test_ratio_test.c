/**
 * @file test_ratio_test.c
 * @brief Unit tests for cxf_ratio_test — entering direction for free variables.
 *
 * Validates that free/superbasic variables get direction from reduced cost
 * sign, not the default +1. Bug: convexfeld-66he.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <math.h>

/* Function under test */
int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out,
                   int *status_out, double *theta_out);

/*
 * Test fixture: 2 constraints, 4 variables (2 structural + 2 slacks).
 *
 * Basic vars: slack0 (var2) in row 0, slack1 (var3) in row 1.
 * Nonbasic: var0 at lower, var1 free (superbasic).
 *
 * x_B = [5.0, 5.0], lb=[0,0], ub=[10,10] for slacks.
 */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;

static double work_lb[4], work_ub[4], work_x[4], work_dj[4];
static int var_status[4];
static int basic_vars[2];

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));

    test_env.feasibility_tol = 1e-6;
    test_env.infinity = 1e20;

    test_state.num_vars = 2;
    test_state.num_constrs = 2;
    test_state.basis = &test_basis;

    test_basis.n = 4;
    test_basis.var_status = var_status;

    /* Slacks are basic in rows 0,1 */
    basic_vars[0] = 2;  /* slack0 basic in row 0 */
    basic_vars[1] = 3;  /* slack1 basic in row 1 */
    test_basis.basic_vars = basic_vars;

    /* var0: nonbasic at lower bound */
    var_status[0] = CXF_VAR_AT_LOWER;
    work_lb[0] = 0.0;  work_ub[0] = 10.0;  work_x[0] = 0.0;

    /* var1: free/superbasic */
    var_status[1] = CXF_VAR_SUPERBASIC;
    work_lb[1] = -1e20; work_ub[1] = 1e20;  work_x[1] = 0.0;

    /* slack0 (var2): basic, value 5, bounds [0,10] */
    var_status[2] = 0;  /* basic in row 0 */
    work_lb[2] = 0.0;  work_ub[2] = 10.0;  work_x[2] = 5.0;

    /* slack1 (var3): basic, value 5, bounds [0,10] */
    var_status[3] = 1;  /* basic in row 1 */
    work_lb[3] = 0.0;  work_ub[3] = 10.0;  work_x[3] = 5.0;

    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_x = work_x;
    test_state.work_dj = work_dj;

    work_dj[0] = 0.0;
    work_dj[1] = 0.0;
    work_dj[2] = 0.0;
    work_dj[3] = 0.0;
}

void tearDown(void) {}

/* --- Null argument tests --- */

void test_ratio_test_null_state(void) {
    double pivotCol[2] = {1.0, 1.0};
    int lr; double pe;
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
        cxf_ratio_test(NULL, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL));
}

void test_ratio_test_null_env(void) {
    double pivotCol[2] = {1.0, 1.0};
    int lr; double pe;
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
        cxf_ratio_test(&test_state, NULL, 0, pivotCol, 2, &lr, &pe, NULL, NULL));
}

/* --- Standard variable at lower bound (s=+1 baseline) --- */

void test_ratio_test_at_lower_bound(void) {
    /* Entering var0 from lower bound. pivotCol = [1.0, 2.0].
     * s=+1 → sd_i>0 means basic decreases → hits lower bound.
     * Row 0: (5-0)/1 = 5.0, Row 1: (5-0)/2 = 2.5.
     * Minimum ratio at row 1. */
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Free variable with negative reduced cost (s=+1) --- */

void test_ratio_test_free_var_negative_dj(void) {
    /* Free var1 entering with dj < 0 → should increase → s=+1.
     * Same as at-lower-bound behavior. pivotCol = [1.0, 2.0].
     * Row 0: (5-0)/1 = 5.0, Row 1: (5-0)/2 = 2.5 → row 1 leaves. */
    work_dj[1] = -3.0;
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Free variable with positive reduced cost (s=-1) --- */

void test_ratio_test_free_var_positive_dj(void) {
    /* Free var1 entering with dj > 0 → should decrease → s=-1.
     * pivotCol = [1.0, 2.0]. With s=-1:
     * sd_i = -1*1 = -1 < 0 → basic increases → hits upper bound.
     * Row 0: (10-5)/1 = 5.0, Row 1: (10-5)/2 = 2.5 → row 1 leaves. */
    work_dj[1] = 3.0;
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Free var with positive dj picks different row than negative dj --- */

void test_ratio_test_free_var_direction_affects_leaving(void) {
    /* Asymmetric setup: slack0 near lower, slack1 near upper.
     * With s=+1 (dj<0): basics decrease → row 0 blocks first (near lb).
     * With s=-1 (dj>0): basics increase → row 1 blocks first (near ub). */
    work_x[2] = 1.0;   /* slack0 near lower bound 0 */
    work_x[3] = 9.0;   /* slack1 near upper bound 10 */

    double pivotCol[2] = {1.0, 1.0};
    int lr; double pe;

    /* dj < 0 → s=+1 → basics decrease → slack0 hits lb first */
    work_dj[1] = -2.0;
    lr = -1; pe = 0.0;
    int rc1 = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc1);
    TEST_ASSERT_EQUAL_INT(0, lr);  /* row 0 (slack0) leaves */

    /* dj > 0 → s=-1 → basics increase → slack1 hits ub first */
    work_dj[1] = 2.0;
    lr = -1; pe = 0.0;
    int rc2 = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc2);
    TEST_ASSERT_EQUAL_INT(1, lr);  /* row 1 (slack1) leaves */
}

/* --- At upper bound variable (s=-1 baseline) --- */

void test_ratio_test_at_upper_bound(void) {
    /* Entering var0 from upper bound → s=-1.
     * pivotCol = [1.0, 2.0]. sd_i = -1*d_i < 0 → basics increase.
     * Row 0: (10-5)/1 = 5.0, Row 1: (10-5)/2 = 2.5 → row 1 leaves. */
    var_status[0] = CXF_VAR_AT_UPPER;
    work_x[0] = 10.0;

    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Degenerate pivot (basic variable exactly at bound) --- */

void test_ratio_test_degenerate(void) {
    /* slack0 at lower bound (x=0): ratio = 0 (degenerate).
     * slack1 at 5: ratio = 5/1 = 5. Row 0 still leaves. */
    work_x[2] = 0.0;
    work_x[3] = 5.0;
    double pivotCol[2] = {2.0, 1.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Bland's rule: smallest variable index among tied candidates --- */

void test_ratio_test_bland_smallest_index(void) {
    /* Both slacks at lower bound (degenerate, ratio=0 for both).
     * Row 0 has var 3 (larger index, larger pivot |d|=8).
     * Row 1 has var 2 (smaller index, smaller pivot |d|=2).
     * Without Bland: picks largest pivot (row 0).
     * With Bland: picks smallest variable index (row 1). */
    basic_vars[0] = 3;  basic_vars[1] = 2;
    var_status[2] = 1;  var_status[3] = 0;
    work_x[2] = 0.0;    work_x[3] = 0.0;

    double pivotCol[2] = {8.0, 2.0};
    int lr; double pe;

    test_state.use_bland = 0;
    lr = -1; pe = 0.0;
    cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 8.0, pe);

    test_state.use_bland = 1;
    lr = -1; pe = 0.0;
    cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pe);
}

/* --- Unbounded detection --- */

void test_ratio_test_unbounded(void) {
    /* Free var entering, all pivot col elements zero → unbounded */
    work_dj[1] = -1.0;
    double pivotCol[2] = {0.0, 0.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2, &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, rc);
}

/* --- Status enum: normal vs degenerate --- */

/* --- Infeasibility pre-check (harris_ratio_test.md Edge Cases) --- */

void test_ratio_test_infeasible_bounds(void) {
    /* Set slack0 bounds to lb > ub → should detect infeasibility */
    work_lb[2] = 10.0;
    work_ub[2] = 0.0;  /* lb > ub */
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
}

/* --- Status enum: normal vs degenerate --- */

void test_ratio_test_status_normal(void) {
    /* Basic vars at x=5, bounds [0,10] → step > 0 → NORMAL_PIVOT */
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;
    int status = -1;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2,
                            &lr, &pe, &status, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(CXF_RT_NORMAL_PIVOT, status);
}

void test_ratio_test_status_degenerate(void) {
    /* slack0 exactly at lower bound → step = 0 → DEGENERATE_PIVOT */
    work_x[2] = 0.0;
    double pivotCol[2] = {2.0, 1.0};
    int lr = -1; double pe = 0.0;
    int status = -1;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2,
                            &lr, &pe, &status, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(CXF_RT_DEGENERATE_PIVOT, status);
}

void test_ratio_test_status_unbounded(void) {
    /* All zero pivot col → UNBOUNDED status */
    work_dj[1] = -1.0;
    double pivotCol[2] = {0.0, 0.0};
    int lr = -1; double pe = 0.0;
    int status = -1;

    int rc = cxf_ratio_test(&test_state, &test_env, 1, pivotCol, 2,
                            &lr, &pe, &status, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, rc);
    TEST_ASSERT_EQUAL_INT(CXF_RT_UNBOUNDED, status);
}

/* --- Theta output (V2 harris_ratio_test.md) --- */

void test_ratio_test_theta_output(void) {
    /* Basic vars at x=5, bounds [0,10], pivotCol=[1,2], entering from lb.
     * Leaving row 1 (smallest ratio). theta = (5-0)/2 = 2.5. */
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0;
    double theta = -1.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, &theta);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, lr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.5, theta);
}

void test_ratio_test_theta_degenerate(void) {
    /* slack0 at lower bound → theta = 0. */
    work_x[2] = 0.0;
    double pivotCol[2] = {2.0, 1.0};
    int lr = -1; double pe = 0.0;
    double theta = -1.0;

    int rc = cxf_ratio_test(&test_state, &test_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, &theta);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, theta);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ratio_test_null_state);
    RUN_TEST(test_ratio_test_null_env);
    RUN_TEST(test_ratio_test_at_lower_bound);
    RUN_TEST(test_ratio_test_free_var_negative_dj);
    RUN_TEST(test_ratio_test_free_var_positive_dj);
    RUN_TEST(test_ratio_test_free_var_direction_affects_leaving);
    RUN_TEST(test_ratio_test_degenerate);
    RUN_TEST(test_ratio_test_bland_smallest_index);
    RUN_TEST(test_ratio_test_at_upper_bound);
    RUN_TEST(test_ratio_test_unbounded);
    RUN_TEST(test_ratio_test_infeasible_bounds);
    RUN_TEST(test_ratio_test_status_normal);
    RUN_TEST(test_ratio_test_status_degenerate);
    RUN_TEST(test_ratio_test_status_unbounded);
    RUN_TEST(test_ratio_test_theta_output);
    RUN_TEST(test_ratio_test_theta_degenerate);
    return UNITY_END();
}
