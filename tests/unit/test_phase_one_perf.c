/**
 * @file test_phase_one_perf.c
 * @brief Tests that cxf_setup_phase_one computes correct slack values
 *        using O(nnz) traversal (CSR or CSC), not O(n*nnz) column scan.
 *
 * Issue: convexfeld-h2k0.  Spec: two_phase_method.md.
 * Verifies slack = (rhs - A*x_N) / diag for CSR path, CSC-only path,
 * and no-matrix fallback path.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Function under test */
int cxf_setup_phase_one(SolverState *state);

/*
 * Fixture: 2 structural variables, 2 constraints (2 slacks).
 *
 * A = [ 1  2 ]    rhs = [10]    sense = ['<']
 *     [ 3  4 ]          [20]             ['<']
 *
 * x_0 = 0 (at lb), x_1 = 0 (at lb).
 * slack_i = (rhs_i - row_sum_i) / 1.0 = rhs_i.
 *
 * With x_0=1, x_1=2: slack_0 = (10 - 1*1 - 2*2)/1 = 5
 *                      slack_1 = (20 - 3*1 - 4*2)/1 = 9
 */

static CxfEnv test_env;
static CxfModel test_model;
static SolverState test_state;
static BasisState test_basis;

/* Arrays: indices 0,1 = structural; 2,3 = slacks */
static double work_lb[4], work_ub[4], work_obj[4], work_x[4];
static int var_status[4];
static int basic_vars[2];
static double diag_coeff[2];
static char work_sense[2];
static double work_rhs[2];

/* CSC for A = [[1,2],[3,4]] */
static int64_t csc_col_ptr[3] = {0, 2, 4};
static int csc_row_idx_arr[4] = {0, 1, 0, 1};
static double csc_values_arr[4] = {1.0, 3.0, 2.0, 4.0};

/* CSR for the same matrix */
static int64_t csr_row_ptr_arr[3] = {0, 2, 4};
static int csr_col_idx_arr[4] = {0, 1, 0, 1};
static double csr_values_arr[4] = {1.0, 2.0, 3.0, 4.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_model, 0, sizeof(test_model));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));

    test_env.feasibility_tol = CXF_FEASIBILITY_TOL;
    test_model.env = &test_env;
    test_model.obj_coeffs = work_obj;
    test_model.num_vars = 2;
    test_model.num_constrs = 2;
    test_model.matrix = NULL;

    test_state.model_ref = &test_model;
    test_state.num_vars = 2;
    test_state.num_constrs = 2;
    test_state.basis = &test_basis;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;
    test_state.work_sense = work_sense;
    test_state.work_rhs = work_rhs;
    test_state.row_scale = NULL;
    test_state.col_scale = NULL;

    /* Default: CSC only (no CSR) */
    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx_arr;
    test_state.csc_values = csc_values_arr;
    test_state.csr_row_ptr = NULL;
    test_state.csr_col_idx = NULL;
    test_state.csr_values = NULL;

    test_basis.var_status = var_status;
    test_basis.basic_vars = basic_vars;
    test_basis.diag_coeff = diag_coeff;
    test_basis.n = 4;

    work_lb[0] = 0.0;  work_ub[0] = 100.0;
    work_lb[1] = 0.0;  work_ub[1] = 100.0;
    work_sense[0] = '<';  work_sense[1] = '<';
    work_rhs[0] = 10.0;   work_rhs[1] = 20.0;
}

void tearDown(void) {}

/**
 * Test 1: CSC-only path produces correct slacks when x_N = 0.
 * slack_i = rhs_i / 1.0.
 */
void test_csc_path_zero_nonbasic(void) {
    cxf_setup_phase_one(&test_state);

    /* x_0=0, x_1=0, so slack = rhs / diag = rhs / 1 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, test_state.work_x[3]);
}

/**
 * Test 2: CSC-only path with nonzero x_N.
 * Set lb[0]=1, lb[1]=2. Then x_0=1, x_1=2.
 * slack_0 = (10 - 1*1 - 2*2) / 1 = 5
 * slack_1 = (20 - 3*1 - 4*2) / 1 = 9
 */
void test_csc_path_nonzero_nonbasic(void) {
    work_lb[0] = 1.0;
    work_lb[1] = 2.0;

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 9.0, test_state.work_x[3]);
}

/**
 * Test 3: CSR path produces identical results.
 */
void test_csr_path_nonzero_nonbasic(void) {
    test_state.csr_row_ptr = csr_row_ptr_arr;
    test_state.csr_col_idx = csr_col_idx_arr;
    test_state.csr_values = csr_values_arr;

    work_lb[0] = 1.0;
    work_lb[1] = 2.0;

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 9.0, test_state.work_x[3]);
}

/**
 * Test 4: CSR path with zero nonbasics.
 */
void test_csr_path_zero_nonbasic(void) {
    test_state.csr_row_ptr = csr_row_ptr_arr;
    test_state.csr_col_idx = csr_col_idx_arr;
    test_state.csr_values = csr_values_arr;

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, test_state.work_x[3]);
}

/**
 * Test 5: No-matrix fallback (no CSC, no CSR). Slack = rhs / diag.
 */
void test_no_matrix_fallback(void) {
    test_state.csc_col_ptr = NULL;
    test_state.csc_row_idx = NULL;
    test_state.csc_values = NULL;

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, test_state.work_x[3]);
}

/**
 * Test 6: GE sense gives diag_coeff = -1.
 * slack = (rhs - row_sum) / (-1).
 * With x_N=0: slack = rhs / -1 = -rhs.
 */
void test_ge_sense_diag_coeff(void) {
    work_sense[0] = '>';
    work_sense[1] = '>';

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -10.0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -20.0, test_state.work_x[3]);
}

/**
 * Test 7: CSR and CSC agree on a GE-sense problem with nonzero x_N.
 */
void test_csr_csc_agree_ge_sense(void) {
    work_sense[0] = '>';
    work_sense[1] = '<';
    work_lb[0] = 1.0;
    work_lb[1] = 2.0;

    /* CSC-only path */
    cxf_setup_phase_one(&test_state);
    double slack_csc_0 = test_state.work_x[2];
    double slack_csc_1 = test_state.work_x[3];

    /* CSR path */
    test_state.csr_row_ptr = csr_row_ptr_arr;
    test_state.csr_col_idx = csr_col_idx_arr;
    test_state.csr_values = csr_values_arr;

    cxf_setup_phase_one(&test_state);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, slack_csc_0, test_state.work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, slack_csc_1, test_state.work_x[3]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_csc_path_zero_nonbasic);
    RUN_TEST(test_csc_path_nonzero_nonbasic);
    RUN_TEST(test_csr_path_nonzero_nonbasic);
    RUN_TEST(test_csr_path_zero_nonbasic);
    RUN_TEST(test_no_matrix_fallback);
    RUN_TEST(test_ge_sense_diag_coeff);
    RUN_TEST(test_csr_csc_agree_ge_sense);
    return UNITY_END();
}
