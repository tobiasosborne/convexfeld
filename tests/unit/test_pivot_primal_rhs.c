/**
 * @file test_pivot_primal_rhs.c
 * @brief Tests that cxf_pivot_primal modifies work_rhs, not model RHS.
 *
 * V2 spec (pivot_operations.md) requires working copies for all solver
 * mutations. The original model data (matrix->rhs) must be preserved.
 * Issue: convexfeld-ir0d.
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

/* Test fixtures */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;
static CxfModel test_model;
static MatrixData test_matrix;

/* Working arrays (2 vars, 2 constraints) */
static double work_lb[2], work_ub[2], work_obj[2], work_x[2];
static int var_status[2];
static double work_rhs[2];

/* CSC: var0 has coeff 0.03 in row0 and 0.015 in row1;
 *      var1 has coeff -0.02 in row0.
 * Small coefficients so T2.14 guard (max_coeff*range <= tol) passes. */
static int64_t csc_col_ptr[3] = {0, 2, 3};
static int csc_row_idx[3]     = {0, 1, 0};
static double csc_values[3]   = {0.03, 0.015, -0.02};

/* Original model RHS (must never be modified) */
static double model_rhs[2] = {100.0, 200.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));
    memset(&test_model, 0, sizeof(test_model));
    memset(&test_matrix, 0, sizeof(test_matrix));

    test_state.num_vars = 2;
    test_state.num_constrs = 2;
    test_state.basis = &test_basis;

    test_basis.n = 2;
    test_basis.var_status = var_status;
    var_status[0] = CXF_VAR_AT_LOWER;
    var_status[1] = CXF_VAR_AT_LOWER;

    /* Bounds: wide enough to not trigger infeasibility */
    work_lb[0] = 0.0;  work_ub[0] = 10.0;
    work_lb[1] = 0.0;  work_ub[1] = 10.0;

    /* Objective: positive RC -> fix at lower bound */
    work_obj[0] = 5.0;
    work_obj[1] = -3.0;
    memset(work_x, 0, sizeof(work_x));

    test_state.obj_value = 0.0;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;

    /* Reset CSC data (T2.13 invalidates csc_row_idx during pivot) */
    csc_row_idx[0] = 0; csc_row_idx[1] = 1; csc_row_idx[2] = 0;
    csc_values[0] = 0.03; csc_values[1] = 0.015; csc_values[2] = -0.02;

    /* Working CSC (SolverState owns these) */
    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx;
    test_state.csc_values  = csc_values;

    /* Working RHS (SolverState copy) */
    work_rhs[0] = 100.0;
    work_rhs[1] = 200.0;
    test_state.work_rhs = work_rhs;

    /* Original model RHS (must remain untouched) */
    model_rhs[0] = 100.0;
    model_rhs[1] = 200.0;
    test_matrix.rhs = model_rhs;
    test_matrix.col_ptr = csc_col_ptr;
    test_matrix.row_idx = csc_row_idx;
    test_matrix.values  = csc_values;
    test_matrix.num_cols = 2;
    test_matrix.num_rows = 2;
    test_model.matrix = &test_matrix;
    test_state.model_ref = &test_model;
}

void tearDown(void) {}

/*--- Core test: model RHS preserved after pivot ---*/

void test_model_rhs_unchanged_after_pivot(void) {
    /* Pivot var1: RC=-3.0 (negative) -> fix at ub=10.0
     * var1 column: coeff -2.0 in row0
     * Expected: work_rhs[0] -= (-2.0)*10.0 = work_rhs[0] + 20.0 = 120.0
     * Model RHS must remain 100.0 and 200.0 */
    int rc = cxf_pivot_primal(&test_env, &test_state, 1, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Model RHS must be completely untouched */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 100.0, model_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 200.0, model_rhs[1]);
}

void test_work_rhs_updated_correctly(void) {
    /* Pivot var1: fix at ub=10.0
     * var1 column: coeff -0.02 in row0
     * work_rhs[0] = 100.0 - (-0.02)*10.0 = 100.2
     * work_rhs[1] unchanged = 200.0
     * Tolerance 1.0 so both T2.15 (2*1=2 < 10) and T2.14 (0.02*10=0.2 < 1) pass. */
    int rc = cxf_pivot_primal(&test_env, &test_state, 1, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 100.2, work_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 200.0, work_rhs[1]);
}

void test_model_rhs_unchanged_multi_row_pivot(void) {
    /* Pivot var0: RC=5.0 (positive) -> fix at lb=0.0
     * var0 column: 3.0 in row0, 1.5 in row1
     * pivotValue=0 -> no RHS change expected (0*coeff=0)
     * Model RHS must still be untouched */
    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 100.0, model_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 200.0, model_rhs[1]);
}

void test_model_rhs_unchanged_upper_bound_pivot(void) {
    /* Set var0 RC negative to force fix at upper bound */
    work_obj[0] = -4.0;

    /* Pivot var0: RC=-4.0 -> fix at ub=10.0
     * var0 column: 0.03 in row0, 0.015 in row1
     * work_rhs[0] -= 0.03*10.0  => 100 - 0.3 = 99.7
     * work_rhs[1] -= 0.015*10.0 => 200 - 0.15 = 199.85
     * Model RHS must remain 100 and 200 */
    int rc = cxf_pivot_primal(&test_env, &test_state, 0, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Model RHS untouched */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 100.0, model_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 200.0, model_rhs[1]);

    /* Working RHS correctly updated */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 99.7, work_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 199.85, work_rhs[1]);
}

void test_sequential_pivots_never_touch_model_rhs(void) {
    /* Pivot var1 first, then var0 — two sequential pivots */
    work_obj[0] = -2.0;  /* negative RC -> fix at ub=10 */
    work_obj[1] = -3.0;  /* negative RC -> fix at ub=10 */

    int rc1 = cxf_pivot_primal(&test_env, &test_state, 1, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc1);

    int rc2 = cxf_pivot_primal(&test_env, &test_state, 0, 1.0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc2);

    /* Model RHS must be completely untouched after both pivots */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 100.0, model_rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 200.0, model_rhs[1]);
}

/*--- Main ---*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_model_rhs_unchanged_after_pivot);
    RUN_TEST(test_work_rhs_updated_correctly);
    RUN_TEST(test_model_rhs_unchanged_multi_row_pivot);
    RUN_TEST(test_model_rhs_unchanged_upper_bound_pivot);
    RUN_TEST(test_sequential_pivots_never_touch_model_rhs);

    return UNITY_END();
}
