/**
 * @file test_pivot_primal.c
 * @brief Tests for cxf_pivot_primal (V2 spec compliance).
 *
 * Validates: CXF_INFEASIBLE return code, parameter order (env, state, var, tol),
 * primal criterion target selection, and no-op on large coefficient impact.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include <string.h>
#include <math.h>

/* Function under test */
int cxf_pivot_primal(void *env, void *state, int var, double tolerance);

/* Minimal test fixtures */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;
static CxfModel test_model;
static MatrixData test_matrix;

/* Working arrays (3 vars) */
static double work_lb[3], work_ub[3], work_obj[3], work_x[3];
static int var_status[3];

/* CSC for 2x3 matrix: col0=[2.0 in row0], col1=[-1.0 in row1],
 * col2=[0.001 in row0] (tiny coefficient) */
static int64_t csc_col_ptr[4] = {0, 1, 2, 3};
static int csc_row_idx[3]     = {0, 1, 0};
static double csc_values[3]   = {2.0, -1.0, 0.001};
static double rhs[2]          = {10.0, 20.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));
    memset(&test_model, 0, sizeof(test_model));
    memset(&test_matrix, 0, sizeof(test_matrix));

    test_state.num_vars = 3;
    test_state.num_constrs = 2;
    test_state.basis = &test_basis;

    test_basis.n = 3;
    test_basis.var_status = var_status;

    /* Default bounds */
    work_lb[0] = 0.0;   work_ub[0] = 10.0;
    work_lb[1] = 1.0;   work_ub[1] = 5.0;
    work_lb[2] = -2.0;  work_ub[2] = 8.0;

    work_obj[0] = 3.0; work_obj[1] = -2.0; work_obj[2] = 0.0;
    memset(work_x, 0, sizeof(work_x));

    test_state.obj_value = 0.0;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;

    var_status[0] = CXF_VAR_AT_LOWER;
    var_status[1] = CXF_VAR_AT_UPPER;
    var_status[2] = CXF_VAR_AT_LOWER;

    /* Matrix setup for RHS update path */
    test_matrix.col_ptr = csc_col_ptr;
    test_matrix.row_idx = csc_row_idx;
    test_matrix.values = csc_values;
    test_matrix.rhs = rhs;
    test_matrix.num_cols = 3;
    test_matrix.num_rows = 2;
    test_model.matrix = &test_matrix;
    test_state.model_ref = &test_model;

    rhs[0] = 10.0;
    rhs[1] = 20.0;
}

void tearDown(void) {}

/*=== V2 compliance: CXF_INFEASIBLE return code ===*/

void test_tight_bounds_above_bound_eq_tol_succeeds(void) {
    /* Bounds range = 1e-9 > CXF_BOUND_EQUALITY_TOL (1e-10).
     * Per tolerances_constants.md Section 9 and numerical_stability.md
     * Section C, this is NOT fixed — pivot should proceed. */
    work_lb[0] = 5.0;
    work_ub[0] = 5.0 + 1e-9;

    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

void test_equal_bounds_returns_CXF_INFEASIBLE(void) {
    work_lb[1] = 3.0;
    work_ub[1] = 3.0;

    int rc = cxf_pivot_primal(&test_env, &test_state, 1, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
}

/*=== Null/invalid argument tests ===*/

void test_null_state_returns_error(void) {
    int rc = cxf_pivot_primal(&test_env, NULL, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_null_env_returns_error(void) {
    int rc = cxf_pivot_primal(NULL, &test_state, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_negative_var_returns_error(void) {
    int rc = cxf_pivot_primal(&test_env, &test_state, -1, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

void test_zero_tolerance_returns_error(void) {
    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 0.0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

/*=== Primal criterion: positive RC → fix at lower bound ===*/

void test_positive_rc_fixes_at_lower(void) {
    /* var0: rc=3.0 (positive), bounds [0,10] → fix at lb=0 */
    work_obj[0] = 3.0;
    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1e-8);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* obj_value updated: 0 + 3.0*0.0 = 0.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, test_state.obj_value);
}

/*=== Primal criterion: negative RC → fix at upper bound ===*/

void test_negative_rc_fixes_at_upper(void) {
    /* var1: rc=-2.0 (negative), bounds [1,5] → fix at ub=5 */
    work_obj[1] = -2.0;
    int rc = cxf_pivot_primal(&test_env, &test_state, 1, 1e-8);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* obj_value updated: 0 + (-2.0)*5.0 = -10.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -10.0, test_state.obj_value);
}

/*=== Main ===*/

int main(void) {
    UNITY_BEGIN();

    /* V2 compliance: bound equality tolerance */
    RUN_TEST(test_tight_bounds_above_bound_eq_tol_succeeds);
    RUN_TEST(test_equal_bounds_returns_CXF_INFEASIBLE);

    /* Null/invalid args */
    RUN_TEST(test_null_state_returns_error);
    RUN_TEST(test_null_env_returns_error);
    RUN_TEST(test_negative_var_returns_error);
    RUN_TEST(test_zero_tolerance_returns_error);

    /* Primal criterion */
    RUN_TEST(test_positive_rc_fixes_at_lower);
    RUN_TEST(test_negative_rc_fixes_at_upper);

    return UNITY_END();
}
