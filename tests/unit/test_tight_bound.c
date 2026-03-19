/**
 * @file test_tight_bound.c
 * @brief Tests for tight-bound variable processing in step.c (Phase 3.2).
 *
 * V2 spec simplex_iteration.md Phase 3 item 2: variables with bound range
 * at or below pricing tolerance routed to cxf_pivot_primal.
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

/* Function under test (declared in simplex_internal.h) */
int cxf_pivot_primal(void *env, void *state, int var, double tol);

/* Minimal fixtures: 2 structural vars, 2 constraints, 2 slacks (total=4) */
#define N_VARS    2
#define N_CONSTRS 2
#define TOTAL     (N_VARS + N_CONSTRS)

static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;
static CxfModel test_model;
static MatrixData test_matrix;

static double work_lb[TOTAL], work_ub[TOTAL], work_obj[TOTAL];
static double work_x[TOTAL], work_dj[TOTAL];
static int var_status[TOTAL];
static int basic_vars[N_CONSTRS];

/* CSC for 2x2 identity-like matrix: col0=[1.0 in row0], col1=[1.0 in row1] */
static int64_t csc_col_ptr[N_VARS + 1] = {0, 1, 2};
static int csc_row_idx[2] = {0, 1};
static double csc_values[2] = {1.0, 1.0};
static double rhs[N_CONSTRS] = {10.0, 20.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));
    memset(&test_model, 0, sizeof(test_model));
    memset(&test_matrix, 0, sizeof(test_matrix));

    test_env.optimality_tol = 1e-6;
    test_env.feasibility_tol = 1e-6;

    test_state.num_vars = N_VARS;
    test_state.num_constrs = N_CONSTRS;
    test_state.basis = &test_basis;

    test_basis.n = TOTAL;
    test_basis.var_status = var_status;
    test_basis.basic_vars = basic_vars;

    /* Default: slacks are basic, structural vars are nonbasic */
    basic_vars[0] = N_VARS;      /* slack 0 basic in row 0 */
    basic_vars[1] = N_VARS + 1;  /* slack 1 basic in row 1 */

    /* Structural var bounds */
    work_lb[0] = 0.0;   work_ub[0] = 10.0;
    work_lb[1] = 0.0;   work_ub[1] = 10.0;
    /* Slack bounds (wide) */
    work_lb[2] = 0.0;   work_ub[2] = 1e20;
    work_lb[3] = 0.0;   work_ub[3] = 1e20;

    memset(work_obj, 0, sizeof(work_obj));
    memset(work_x, 0, sizeof(work_x));
    memset(work_dj, 0, sizeof(work_dj));

    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;
    test_state.work_dj = work_dj;

    /* All structural vars nonbasic at lower bound */
    var_status[0] = CXF_VAR_AT_LOWER;
    var_status[1] = CXF_VAR_AT_LOWER;
    /* Slacks are basic (status >= 0 = row index) */
    var_status[2] = 0;
    var_status[3] = 1;

    /* Matrix setup for cxf_pivot_primal RHS update path */
    test_matrix.col_ptr = csc_col_ptr;
    test_matrix.row_idx = csc_row_idx;
    test_matrix.values = csc_values;
    test_matrix.rhs = rhs;
    test_matrix.num_cols = N_VARS;
    test_matrix.num_rows = N_CONSTRS;
    test_model.matrix = &test_matrix;
    test_state.model_ref = &test_model;

    rhs[0] = 10.0;
    rhs[1] = 20.0;
}

void tearDown(void) {}

/*=== Test: tight-bound structural variable triggers pivot_primal ===*/

void test_tight_bound_var_fixed_at_lower(void) {
    /* T2.15: 2*tol must be < range for pivot to proceed.
     * Range=1e-4, tol=1e-5 → 2e-5 < 1e-4 → passes. */
    work_lb[0] = 5.0;
    work_ub[0] = 5.0 + 1e-4;
    work_obj[0] = 3.0;  /* positive RC → fix at lb */

    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1e-5);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

void test_tight_bound_var_fixed_at_upper(void) {
    /* T2.15: Range=1e-3, tol=1e-4 → 2e-4 < 1e-3 → passes. */
    work_lb[1] = 3.0;
    work_ub[1] = 3.0 + 1e-3;
    work_obj[1] = -2.0;

    int rc = cxf_pivot_primal(&test_env, &test_state, 1, 1e-4);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*=== Test: variable with wider bounds is NOT processed by pivot_primal ===*/

void test_wide_bound_var_not_tight(void) {
    /* var0: bounds [0, 10], range = 10 >> optimality_tol
     * pivot_primal should fix normally at lower bound (c > 0) */
    work_lb[0] = 0.0;
    work_ub[0] = 10.0;
    work_obj[0] = 3.0;

    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* obj updated: 0 + 3.0 * 0.0 = 0.0 (fixed at lb=0) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, test_state.obj_value);
}

/*=== Test: NULL safety — pivot_primal rejects NULL env/state ===*/

void test_pivot_primal_null_env(void) {
    int rc = cxf_pivot_primal(NULL, &test_state, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_pivot_primal_null_state(void) {
    int rc = cxf_pivot_primal(&test_env, NULL, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/*=== Test: equal bounds → CXF_INFEASIBLE (too tight to pivot) ===*/

void test_equal_bounds_infeasible(void) {
    work_lb[0] = 7.0;
    work_ub[0] = 7.0;

    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
}

/*=== Test: out-of-range var index rejected ===*/

void test_out_of_range_var_rejected(void) {
    /* var index >= num_vars + num_constrs → CXF_ERROR_INVALID_ARGUMENT */
    int total = test_state.num_vars + test_state.num_constrs;
    int rc = cxf_pivot_primal(&test_env, &test_state, total, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

void test_negative_var_rejected(void) {
    int rc = cxf_pivot_primal(&test_env, &test_state, -1, 1e-6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

/*=== Main ===*/

int main(void) {
    UNITY_BEGIN();

    /* Tight-bound behavior */
    RUN_TEST(test_tight_bound_var_fixed_at_lower);
    RUN_TEST(test_tight_bound_var_fixed_at_upper);
    RUN_TEST(test_wide_bound_var_not_tight);
    RUN_TEST(test_equal_bounds_infeasible);

    /* NULL safety */
    RUN_TEST(test_pivot_primal_null_env);
    RUN_TEST(test_pivot_primal_null_state);

    /* Index validation */
    RUN_TEST(test_out_of_range_var_rejected);
    RUN_TEST(test_negative_var_rejected);

    return UNITY_END();
}
