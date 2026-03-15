/**
 * @file test_phase_transition_refactor.c
 * @brief Tests that Phase I->II transition refactorizes and recomputes xB.
 *
 * V2 spec (numerical_stability.md Section A item 4): Phase I to Phase II
 * transition must force refactorization to ensure clean numerical state.
 * Issue: convexfeld-aznn.
 *
 * Test LP:  min 3x1 - 2x2
 *            x1 +  x2 <= 4   (slack s1 = var 2)
 *           2x1 +  x2 <= 6   (slack s2 = var 3)
 *           x1, x2 >= 0
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <math.h>

/* Internal functions */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_transition_to_phase_two(SolverState *state, CxfModel *model);

/* CSC of structural matrix A (2x2):
 *   col 0 (x1): [1.0 in row 0, 2.0 in row 1]
 *   col 1 (x2): [1.0 in row 0, 1.0 in row 1] */
static int64_t csc_col_ptr[] = {0, 2, 4};
static int     csc_row_idx[] = {0, 1, 0, 1};
static double  csc_values[]  = {1.0, 2.0, 1.0, 1.0};

#define M 2
#define N 2
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

/** Build slack basis (B = I), factorize, wire into state. */
static void build_slack_basis(void) {
    basis = cxf_basis_create(M, TOTAL);
    TEST_ASSERT_NOT_NULL(basis);

    /* Slack basis: s1 (var 2) basic in row 0, s2 (var 3) basic in row 1 */
    basis->basic_vars[0] = 2;
    basis->basic_vars[1] = 3;
    basis->var_status[0] = CXF_VAR_AT_LOWER;
    basis->var_status[1] = CXF_VAR_AT_LOWER;
    basis->var_status[2] = 0;  /* basic in row 0 */
    basis->var_status[3] = 1;  /* basic in row 1 */
    for (int i = 0; i < M; i++) basis->diag_coeff[i] = 1.0;

    /* Wire basis into state BEFORE factorize (lu_factorize reads state) */
    state.basis = basis;

    basis->lu = cxf_lu_create(M, 10, 10);
    TEST_ASSERT_NOT_NULL(basis->lu);
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_lu_factorize(basis->lu, &state));
}

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_model, 0, sizeof(test_model));
    memset(&state, 0, sizeof(state));

    test_env.feasibility_tol = 1e-6;
    test_env.optimality_tol = 1e-6;

    obj_coeffs[0] = 3.0;
    obj_coeffs[1] = -2.0;
    test_model.env = &test_env;
    test_model.obj_coeffs = obj_coeffs;
    test_model.num_vars = N;
    test_model.num_constrs = M;

    state.model_ref = &test_model;
    state.num_vars = N;
    state.num_constrs = M;
    state.phase = 1;  /* Currently in Phase I */
    state.csc_col_ptr = csc_col_ptr;
    state.csc_row_idx = csc_row_idx;
    state.csc_values = csc_values;
    state.work_lb = work_lb;
    state.work_ub = work_ub;
    state.work_x = work_x;
    state.work_obj = work_obj;
    state.work_dj = work_dj;
    state.work_rhs = work_rhs;
    state.work_column = work_column;
    state.work_cB = work_cB;
    state.work_sense = work_sense;

    /* Structural bounds: x1, x2 in [0, 10] */
    work_lb[0] = 0; work_lb[1] = 0;
    work_ub[0] = 10; work_ub[1] = 10;
    /* Slack bounds: s1, s2 in [0, inf) */
    work_lb[2] = 0; work_lb[3] = 0;
    work_ub[2] = CXF_INFINITY; work_ub[3] = CXF_INFINITY;

    work_rhs[0] = 4.0;
    work_rhs[1] = 6.0;
    work_sense[0] = '<';
    work_sense[1] = '<';

    /* Phase I w-coefficients (will be replaced by transition) */
    memset(work_obj, 0, sizeof(work_obj));
    memset(work_dj, 0, sizeof(work_dj));

    basis = NULL;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
}

/**
 * Test 1: x_B values are recomputed from scratch during transition.
 *
 * Set basic variables (slacks) to deliberately wrong values, then
 * call cxf_transition_to_phase_two. The recomputation should fix them
 * to the true values: x_B = B^{-1}(b - Nx_N) = [4, 6] when x_N = 0.
 */
void test_transition_recomputes_xB(void) {
    build_slack_basis();

    /* Set nonbasic x1=0, x2=0.  True slacks = [4, 6]. */
    work_x[0] = 0.0;
    work_x[1] = 0.0;
    /* Deliberately wrong basic variable values (simulating drift) */
    work_x[2] = 99.0;
    work_x[3] = 99.0;

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* After transition, x_B should be recomputed to true values */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, work_x[3]);
}

/**
 * Test 2: Phase transitions to phase 2 with correct objective.
 *
 * After recomputing x_B = [4, 6] and restoring obj = [3, -2, 0, 0]:
 * objective = 3*0 + (-2)*0 + 0*4 + 0*6 = 0.
 */
void test_transition_objective_uses_fresh_xB(void) {
    build_slack_basis();

    work_x[0] = 0.0;
    work_x[1] = 0.0;
    work_x[2] = 99.0;  /* drifted */
    work_x[3] = 99.0;  /* drifted */
    state.obj_value = 999.0;  /* stale */

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* obj = 3*0 + (-2)*0 + 0*4 + 0*6 = 0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, state.obj_value);
}

/**
 * Test 3: Phase flag transitions from 1 to 2.
 */
void test_transition_sets_phase_two(void) {
    build_slack_basis();

    work_x[0] = 0.0;
    work_x[1] = 0.0;
    work_x[2] = 4.0;
    work_x[3] = 6.0;
    state.phase = 1;

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, state.phase);
}

/**
 * Test 4: Objective coefficients restored to original model values.
 */
void test_transition_restores_original_objective(void) {
    build_slack_basis();

    work_x[0] = 0.0;
    work_x[1] = 0.0;
    work_x[2] = 4.0;
    work_x[3] = 6.0;

    /* Phase I w-coefficients */
    work_obj[0] = 0.0;
    work_obj[1] = 0.0;
    work_obj[2] = -1.0;
    work_obj[3] = 0.0;

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Should be restored to model objective */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, work_obj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -2.0, work_obj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, work_obj[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, work_obj[3]);
}

/**
 * Test 5: Nonbasic at upper bound -- xB recomputation accounts for it.
 *
 * x2 at upper bound = 10.  True slacks:
 *   s1 = b1 - a12*x2 = 4 - 1*10 = -6
 *   s2 = b2 - a22*x2 = 6 - 1*10 = -4
 */
void test_transition_recomputes_with_nonbasic_at_upper(void) {
    build_slack_basis();

    work_x[0] = 0.0;
    work_x[1] = 10.0;
    basis->var_status[1] = CXF_VAR_AT_UPPER;
    work_x[2] = 99.0;  /* drifted */
    work_x[3] = 99.0;  /* drifted */

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -6.0, work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -4.0, work_x[3]);
}

/**
 * Test 6: Anti-cycling state is reset at transition.
 */
void test_transition_resets_anticycling(void) {
    build_slack_basis();

    work_x[0] = 0.0;
    work_x[1] = 0.0;
    work_x[2] = 4.0;
    work_x[3] = 6.0;

    state.use_bland = 1;
    state.degenerate_count = 500;

    int rc = cxf_transition_to_phase_two(&state, &test_model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, state.use_bland);
    TEST_ASSERT_EQUAL_INT(0, state.degenerate_count);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_transition_recomputes_xB);
    RUN_TEST(test_transition_objective_uses_fresh_xB);
    RUN_TEST(test_transition_sets_phase_two);
    RUN_TEST(test_transition_restores_original_objective);
    RUN_TEST(test_transition_recomputes_with_nonbasic_at_upper);
    RUN_TEST(test_transition_resets_anticycling);

    return UNITY_END();
}
