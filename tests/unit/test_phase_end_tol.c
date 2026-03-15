/**
 * @file test_phase_end_tol.c
 * @brief Tests that cxf_check_phase_one_end never mutates env tolerances.
 *
 * Issue: convexfeld-eph4.  The old code temporarily set
 * env->optimality_tol *= 0.01 then restored it.  The fix passes a
 * local tighter value to the internal helper instead.
 *
 * V2 spec ref: two_phase_method.md line ~142.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <math.h>

/* Functions under test / needed for setup */
int cxf_check_phase_one_end(SolverState *state, CxfModel *model, CxfEnv *env);
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

#define M 1
#define N 1
#define TOTAL (N + M)

static CxfEnv test_env;
static CxfModel test_model;
static SolverState state;
static BasisState *basis;

static double work_lb[TOTAL], work_ub[TOTAL], work_x[TOTAL];
static double work_obj[TOTAL], work_dj[TOTAL], work_rhs[M];
static double work_column[M], work_cB[M];
static double obj_coeffs[N];
static char work_sense[M];
static double min_act[M], max_act[M];

/* 1x1 identity CSC (one structural column with coeff 1 in row 0) */
static int64_t csc_col_ptr[] = {0, 1};
static int     csc_row_idx[] = {0};
static double  csc_values[]  = {1.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_model, 0, sizeof(test_model));
    memset(&state, 0, sizeof(state));

    test_env.feasibility_tol   = 1e-6;
    test_env.optimality_tol    = 1e-6;
    test_env.refactor_interval = 100;

    obj_coeffs[0] = 1.0;
    test_model.env        = &test_env;
    test_model.obj_coeffs = obj_coeffs;
    test_model.num_vars   = N;
    test_model.num_constrs = M;

    /* Build basis via allocator (gives work/work2, lu=NULL) */
    basis = cxf_basis_create(M, TOTAL);

    /* Slack basis: s0 (var 1) basic in row 0 */
    basis->basic_vars[0] = 1;
    basis->var_status[0] = CXF_VAR_AT_LOWER;
    basis->var_status[1] = 0;  /* basic row 0 */
    basis->diag_coeff[0] = 1.0;

    state.model_ref    = &test_model;
    state.num_vars     = N;
    state.num_constrs  = M;
    state.basis        = basis;
    state.phase        = 1;
    state.pricing      = NULL;

    state.work_lb      = work_lb;
    state.work_ub      = work_ub;
    state.work_x       = work_x;
    state.work_obj     = work_obj;
    state.work_dj      = work_dj;
    state.work_rhs     = work_rhs;
    state.work_column  = work_column;
    state.work_cB      = work_cB;
    state.work_sense   = work_sense;
    state.min_activity = min_act;
    state.max_activity = max_act;

    state.csc_col_ptr  = csc_col_ptr;
    state.csc_row_idx  = csc_row_idx;
    state.csc_values   = csc_values;

    /* Structural x0: nonbasic at lower, bounds [0,10] */
    work_lb[0] = 0.0;  work_ub[0] = 10.0;
    work_x[0]  = 0.0;  work_dj[0] = 0.0;

    /* Slack s0: basic, bounds [0, inf) */
    work_lb[1] = 0.0;  work_ub[1] = CXF_INFINITY;
    work_x[1]  = 5.0;  work_dj[1] = 0.0;

    work_sense[0] = '<';
    work_rhs[0]   = 5.0;
    work_obj[0]   = 0.0;
    work_obj[1]   = 0.0;
    min_act[0]    = 0.0;
    max_act[0]    = 10.0;

    state.obj_value = 0.0;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
}

/**
 * Test 1: env->optimality_tol unchanged after the near-feasibility path.
 * Force small positive Phase I obj (enters the tight-tolerance branch).
 */
void test_env_optimality_tol_unchanged_infeasible_path(void) {
    work_x[1] = -2e-6;   /* violation 2e-6 > feas_tol 1e-6 */
    state.obj_value = 2e-6;

    double saved = test_env.optimality_tol;
    cxf_check_phase_one_end(&state, &test_model, &test_env);
    TEST_ASSERT_EQUAL_DOUBLE(saved, test_env.optimality_tol);
}

/**
 * Test 2: env->feasibility_tol unchanged (sanity).
 */
void test_env_feasibility_tol_unchanged(void) {
    work_x[1] = -2e-6;
    state.obj_value = 2e-6;

    double saved = test_env.feasibility_tol;
    cxf_check_phase_one_end(&state, &test_model, &test_env);
    TEST_ASSERT_EQUAL_DOUBLE(saved, test_env.feasibility_tol);
}

/**
 * Test 3: Large Phase I obj (skips tight-tol branch). Tol still intact.
 */
void test_env_tol_unchanged_large_objective(void) {
    work_x[1] = -5.0;
    state.obj_value = 5.0;

    double saved_opt  = test_env.optimality_tol;
    double saved_feas = test_env.feasibility_tol;
    cxf_check_phase_one_end(&state, &test_model, &test_env);
    TEST_ASSERT_EQUAL_DOUBLE(saved_opt,  test_env.optimality_tol);
    TEST_ASSERT_EQUAL_DOUBLE(saved_feas, test_env.feasibility_tol);
}

/**
 * Test 4: Non-default env tolerance preserved exactly.
 */
void test_custom_tol_preserved(void) {
    test_env.optimality_tol = 3.14e-7;
    work_x[1] = -2e-6;
    state.obj_value = 2e-6;

    cxf_check_phase_one_end(&state, &test_model, &test_env);
    TEST_ASSERT_EQUAL_DOUBLE(3.14e-7, test_env.optimality_tol);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_env_optimality_tol_unchanged_infeasible_path);
    RUN_TEST(test_env_feasibility_tol_unchanged);
    RUN_TEST(test_env_tol_unchanged_large_objective);
    RUN_TEST(test_custom_tol_preserved);
    return UNITY_END();
}
