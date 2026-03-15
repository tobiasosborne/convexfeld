/**
 * @file test_pivot_check_dir.c
 * @brief Tests for cxf_pivot_check entering-direction parameter.
 *
 * V2 spec (harris_ratio_test.md Stage 1): s = entering direction.
 * Basic var i driven toward lb when s*d_i > 0, toward ub when s*d_i < 0.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <string.h>
#include <math.h>

/* Function under test (new signature with direction) */
extern int cxf_pivot_check(SolverState *state, const double *pivotCol,
                           int entering, int direction, double *theta_max);

/* Fixtures: 2 constraints, 3 variables (2 structural + 1 slack-like) */
static SolverState st;
static BasisState  bs;
static int    basic_vars[2];
static double wlb[3], wub[3], wx[3];

void setUp(void) {
    memset(&st, 0, sizeof(st));
    memset(&bs, 0, sizeof(bs));
    st.num_constrs = 2;
    st.num_vars = 3;
    bs.m = 2;
    bs.basic_vars = basic_vars;
    st.basis = &bs;
    st.work_lb = wlb;
    st.work_ub = wub;
    st.work_x  = wx;

    /* Basic vars: row 0 -> var 0, row 1 -> var 1 */
    basic_vars[0] = 0;
    basic_vars[1] = 1;

    /* Default: x_0=5 in [0,10], x_1=3 in [0,8] */
    wlb[0] = 0.0;  wub[0] = 10.0;  wx[0] = 5.0;
    wlb[1] = 0.0;  wub[1] =  8.0;  wx[1] = 3.0;
    wlb[2] = 0.0;  wub[2] = 10.0;  wx[2] = 0.0;
}

void tearDown(void) {}

/*=========================================================================
 * direction = +1: s*d_i = d_i  (same as old behavior)
 *========================================================================*/

void test_dir_plus1_pos_pivotcol(void) {
    /* d = [2.0, 0.0].  s=+1 => s*d_0=2>0 => toward lb.
     * theta = (5 - 0)/2 = 2.5 */
    double d[2] = {2.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, +1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, theta);
}

void test_dir_plus1_neg_pivotcol(void) {
    /* d = [-4.0, 0.0].  s=+1 => s*d_0=-4<0 => toward ub.
     * theta = (10 - 5)/4 = 1.25 */
    double d[2] = {-4.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, +1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.25, theta);
}

/*=========================================================================
 * direction = -1: s*d_i = -d_i  (reverses which bound blocks)
 *========================================================================*/

void test_dir_minus1_pos_pivotcol(void) {
    /* d = [2.0, 0.0].  s=-1 => s*d_0=-2<0 => toward ub.
     * theta = (10 - 5)/2 = 2.5 */
    double d[2] = {2.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, -1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, theta);
}

void test_dir_minus1_neg_pivotcol(void) {
    /* d = [-4.0, 0.0].  s=-1 => s*d_0=4>0 => toward lb.
     * theta = (5 - 0)/4 = 1.25 */
    double d[2] = {-4.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, -1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.25, theta);
}

/*=========================================================================
 * Direction determines which row is tighter
 *========================================================================*/

void test_dir_selects_min_theta(void) {
    /* d = [1.0, 2.0].  x_0=5 in [0,10], x_1=3 in [0,8].
     * s=+1: row0 => (5-0)/1=5, row1 => (3-0)/2=1.5. Min = 1.5
     * s=-1: row0 => (10-5)/1=5, row1 => (8-3)/2=2.5. Min = 2.5 */
    double d[2] = {1.0, 2.0};
    double theta;

    int rc1 = cxf_pivot_check(&st, d, 2, +1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.5, theta);

    int rc2 = cxf_pivot_check(&st, d, 2, -1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, theta);
}

/*=========================================================================
 * At-bound cases (theta = 0)
 *========================================================================*/

void test_at_lower_bound_dir_plus1(void) {
    /* x_0 = 0 (at lb).  d=[1.0, 0].  s=+1 => toward lb, theta=0 */
    wx[0] = 0.0;
    double d[2] = {1.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, +1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, theta);
}

void test_at_upper_bound_dir_minus1(void) {
    /* x_0 = 10 (at ub).  d=[1.0, 0].  s=-1 => s*d=-1<0 => toward ub.
     * theta = (10 - 10)/1 = 0 */
    wx[0] = 10.0;
    double d[2] = {1.0, 0.0};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, -1, &theta);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, theta);
}

/*=========================================================================
 * Null / edge cases
 *========================================================================*/

void test_null_returns_error(void) {
    double theta;
    int rc = cxf_pivot_check(NULL, NULL, 0, +1, &theta);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_all_below_pivot_tol_unbounded(void) {
    /* All pivotCol entries negligible => unbounded */
    double d[2] = {1e-15, -1e-15};
    double theta;
    int rc = cxf_pivot_check(&st, d, 2, +1, &theta);
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, rc);
}

/*=========================================================================
 * Main
 *========================================================================*/

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dir_plus1_pos_pivotcol);
    RUN_TEST(test_dir_plus1_neg_pivotcol);
    RUN_TEST(test_dir_minus1_pos_pivotcol);
    RUN_TEST(test_dir_minus1_neg_pivotcol);
    RUN_TEST(test_dir_selects_min_theta);
    RUN_TEST(test_at_lower_bound_dir_plus1);
    RUN_TEST(test_at_upper_bound_dir_minus1);
    RUN_TEST(test_null_returns_error);
    RUN_TEST(test_all_below_pivot_tol_unbounded);
    return UNITY_END();
}
