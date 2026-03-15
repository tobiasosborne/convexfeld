/**
 * @file test_phase_one_tol.c
 * @brief Tests that cxf_setup_phase_one uses env feasibility tolerance.
 *
 * V2 spec (two_phase_method.md) requires Phase I bound violation checks
 * to use the environment's feasibility_tol, not the compile-time constant
 * CXF_FEASIBILITY_TOL.  Issue: convexfeld-mh2o.
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
 * Minimal fixture: 1 structural variable, 1 constraint (1 slack).
 * We only care about the w-coefficient assignment logic in Step 4.
 */
static CxfEnv test_env;
static CxfModel test_model;
static SolverState test_state;
static BasisState test_basis;

/* Working arrays: index 0 = structural, index 1 = slack */
static double work_lb[2], work_ub[2], work_obj[2], work_x[2];
static int var_status[2];
static int basic_vars[1];
static double diag_coeff[1];
static char work_sense[1];
static double work_rhs[1];

/* Minimal CSC (1x1 identity so slack computation works) */
static int64_t csc_col_ptr[2] = {0, 1};
static int csc_row_idx_arr[1] = {0};
static double csc_values_arr[1] = {1.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_model, 0, sizeof(test_model));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));

    /* Default: env feasibility_tol = compile-time constant */
    test_env.feasibility_tol = CXF_FEASIBILITY_TOL;

    test_model.env = &test_env;
    test_model.obj_coeffs = work_obj;
    test_model.num_vars = 1;
    test_model.num_constrs = 1;
    test_model.matrix = NULL;

    test_state.model_ref = &test_model;
    test_state.num_vars = 1;
    test_state.num_constrs = 1;
    test_state.basis = &test_basis;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;
    test_state.work_sense = work_sense;
    test_state.work_rhs = work_rhs;
    test_state.row_scale = NULL;
    test_state.col_scale = NULL;

    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx_arr;
    test_state.csc_values = csc_values_arr;

    test_basis.var_status = var_status;
    test_basis.basic_vars = basic_vars;
    test_basis.diag_coeff = diag_coeff;
    test_basis.n = 2;

    /* Defaults: structural x0 at lb=0, slack s0 basic.
     * Sense '<', rhs=5. */
    work_lb[0] = 0.0;  work_ub[0] = 10.0;
    work_x[0] = 0.0;
    var_status[0] = CXF_VAR_AT_LOWER;
    work_obj[0] = 0.0;

    work_sense[0] = '<';
    work_rhs[0] = 5.0;
}

void tearDown(void) {}

/*
 * Test 1: Violation just inside the default CXF_FEASIBILITY_TOL (1e-6)
 * but outside a tighter env tolerance (1e-8).
 * With env->feasibility_tol = 1e-8, this IS a violation (Phase I needed).
 * With compile-time 1e-6, this would NOT be a violation (Phase II).
 */
void test_tight_tol_detects_small_violation(void) {
    test_env.feasibility_tol = 1e-8;  /* Tighter than default 1e-6 */

    /* After setUp, slack s0 is basic.
     * We set slack value to be 5e-7 below lb=0 → violation of 5e-7.
     * This is > 1e-8 (detected) but < 1e-6 (missed with old constant). */
    cxf_setup_phase_one(&test_state);

    /* Slack idx=1, its value = (rhs - a'x) / diag = (5 - 1*0)/1 = 5.
     * That's feasible. So we need to force a violation directly.
     * Re-run with a slack that violates: set rhs so slack < 0. */
    work_rhs[0] = -5e-7;  /* slack = -5e-7 / 1 = -5e-7, below lb=0 */

    cxf_setup_phase_one(&test_state);

    /* Slack s0 (idx=1) value = -5e-7, lb=0. Violation = 5e-7.
     * With env tol 1e-8: 5e-7 > 1e-8 → detected → Phase I. */
    TEST_ASSERT_EQUAL_INT(1, test_state.phase);
    TEST_ASSERT_EQUAL_INT(1, test_state.num_artificials);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -1.0, test_state.work_obj[1]);
}

/*
 * Test 2: Same violation (5e-7), but with a loose tolerance (1e-5).
 * This should NOT be detected → skip to Phase II.
 */
void test_loose_tol_ignores_small_violation(void) {
    test_env.feasibility_tol = 1e-5;  /* Looser than default 1e-6 */

    work_rhs[0] = -5e-7;  /* slack = -5e-7, violation = 5e-7 */

    cxf_setup_phase_one(&test_state);

    /* With tol=1e-5: 5e-7 < 1e-5 → NOT detected → Phase II. */
    TEST_ASSERT_EQUAL_INT(2, test_state.phase);
    TEST_ASSERT_EQUAL_INT(0, test_state.num_artificials);
}

/*
 * Test 3: Upper bound violation just inside default but outside tight tol.
 * Slack at ub+eps where eps < 1e-6 but > 1e-8.
 */
void test_tight_tol_detects_upper_violation(void) {
    test_env.feasibility_tol = 1e-8;

    /* Equality constraint: slack fixed at 0 (lb=0, ub=0). */
    work_sense[0] = '=';
    work_rhs[0] = 5e-7;  /* slack = 5e-7 / 1 = 5e-7, above ub=0 */

    cxf_setup_phase_one(&test_state);

    /* Slack s0 (idx=1) ub=0, value=5e-7. Violation = 5e-7.
     * With tol=1e-8: detected → Phase I, w=+1. */
    TEST_ASSERT_EQUAL_INT(1, test_state.phase);
    TEST_ASSERT_EQUAL_INT(1, test_state.num_artificials);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, +1.0, test_state.work_obj[1]);
}

/*
 * Test 4: Verify env tolerance is actually used, not hardcoded constant.
 * Two calls with identical problem but different env tolerances should
 * produce different outcomes.
 */
void test_tolerance_is_runtime_not_constant(void) {
    /* Violation of exactly 5e-7 below lb */
    work_rhs[0] = -5e-7;

    /* First call: tight tolerance → Phase I */
    test_env.feasibility_tol = 1e-8;
    cxf_setup_phase_one(&test_state);
    TEST_ASSERT_EQUAL_INT(1, test_state.phase);

    /* Reset state for second call */
    test_state.num_artificials = 0;
    test_state.obj_value = 0.0;
    test_state.phase = 0;
    for (int j = 0; j < 2; j++) work_obj[j] = 0.0;

    /* Second call: loose tolerance → Phase II */
    test_env.feasibility_tol = 1e-5;
    cxf_setup_phase_one(&test_state);
    TEST_ASSERT_EQUAL_INT(2, test_state.phase);
}

/*
 * Test 5: Fallback to CXF_FEASIBILITY_TOL when model_ref is NULL.
 * (Defensive: shouldn't happen in practice, but code handles it.)
 */
void test_null_model_ref_uses_default(void) {
    test_state.model_ref = NULL;

    /* With no env, function should fall back to CXF_FEASIBILITY_TOL.
     * Just verify it doesn't crash and produces a result. */
    /* Need manual slack setup since phase_one won't find model_ref.
     * Phase I still runs on the working arrays directly. */
    work_lb[1] = 0.0;
    work_ub[1] = CXF_INFINITY;
    basic_vars[0] = 1;
    var_status[1] = 0;
    diag_coeff[0] = 1.0;
    work_x[1] = -2.0;  /* Clear violation below lb=0 */

    /* We can't call full setup (it accesses model_ref in step 2b).
     * Instead, test that it handles NULL gracefully — no crash. */
    /* The function checks model_ref in step 2b; if NULL, it skips.
     * For Step 4, our fix gracefully falls back. Just verify no crash. */
    int rc = cxf_setup_phase_one(&test_state);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tight_tol_detects_small_violation);
    RUN_TEST(test_loose_tol_ignores_small_violation);
    RUN_TEST(test_tight_tol_detects_upper_violation);
    RUN_TEST(test_tolerance_is_runtime_not_constant);
    RUN_TEST(test_null_model_ref_uses_default);

    return UNITY_END();
}
