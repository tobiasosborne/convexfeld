/**
 * @file test_phase_end_large.c
 * @brief Tests for convexfeld-qxw8: modified[] was fixed-size 256 array.
 *
 * Verifies that cxf_simplex_phase_end handles >256 constraints correctly.
 * The old code used int modified[256] and capped num_modified at 256,
 * silently dropping later constraints from activity bound recomputation.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Function under test */
int cxf_simplex_phase_end(SolverState *state, CxfEnv *env, int doScan);

#define NUM_CONSTRS 400
#define NUM_VARS    1
#define TOTAL       (NUM_VARS + NUM_CONSTRS)

static CxfEnv env;
static SolverState state;
static BasisState basis;
static PricingState pricing;

/* Working arrays (heap-allocated in setUp, freed in tearDown) */
static double *work_lb, *work_ub, *work_obj, *work_x, *work_dj;
static double *work_rhs, *min_act, *max_act;
static char *work_sense;
static int *var_status, *var_dirty;

/* Minimal CSR: each constraint has 1 variable (col 0) with coeff 1.0 */
static int64_t *csr_row_ptr;
static int *csr_col_idx;
static double *csr_values;

/* Minimal CSC: col 0 touches all NUM_CONSTRS rows */
static int64_t csc_col_ptr[NUM_VARS + 1];
static int *csc_row_idx;
static double *csc_values;

void setUp(void) {
    memset(&env, 0, sizeof(env));
    memset(&state, 0, sizeof(state));
    memset(&basis, 0, sizeof(basis));
    memset(&pricing, 0, sizeof(pricing));

    env.feasibility_tol = 1e-6;
    env.optimality_tol = 1e-6;

    state.num_vars = NUM_VARS;
    state.num_constrs = NUM_CONSTRS;
    state.basis = &basis;
    state.phase = 2;

    /* Allocate working arrays */
    work_lb  = calloc(TOTAL, sizeof(double));
    work_ub  = calloc(TOTAL, sizeof(double));
    work_obj = calloc(TOTAL, sizeof(double));
    work_x   = calloc(TOTAL, sizeof(double));
    work_dj  = calloc(TOTAL, sizeof(double));
    work_rhs = calloc(NUM_CONSTRS, sizeof(double));
    work_sense = calloc(NUM_CONSTRS, sizeof(char));
    min_act  = calloc(NUM_CONSTRS, sizeof(double));
    max_act  = calloc(NUM_CONSTRS, sizeof(double));
    var_status = calloc(TOTAL, sizeof(int));
    var_dirty  = calloc(NUM_VARS, sizeof(int));

    /* CSR: identity-like, 1 nonzero per row */
    csr_row_ptr = calloc(NUM_CONSTRS + 1, sizeof(int64_t));
    csr_col_idx = calloc(NUM_CONSTRS, sizeof(int));
    csr_values  = calloc(NUM_CONSTRS, sizeof(double));

    /* CSC: col 0 touches all rows */
    csc_row_idx = calloc(NUM_CONSTRS, sizeof(int));
    csc_values  = calloc(NUM_CONSTRS, sizeof(double));

    for (int i = 0; i < NUM_CONSTRS; i++) {
        csr_row_ptr[i] = i;
        csr_col_idx[i] = 0;
        csr_values[i] = 1.0;
        csc_row_idx[i] = i;
        csc_values[i] = 1.0;
    }
    csr_row_ptr[NUM_CONSTRS] = NUM_CONSTRS;
    csc_col_ptr[0] = 0;
    csc_col_ptr[1] = NUM_CONSTRS;

    basis.n = TOTAL;
    basis.var_status = var_status;

    /* Single structural var: nonbasic at lower bound, range [0, 10] */
    work_lb[0] = 0.0;
    work_ub[0] = 10.0;
    work_x[0] = 0.0;
    var_status[0] = CXF_VAR_AT_LOWER;

    /* All slack vars: nonbasic at lower bound */
    for (int i = 0; i < NUM_CONSTRS; i++) {
        int si = NUM_VARS + i;
        work_lb[si] = 0.0;
        work_ub[si] = CXF_INFINITY;
        var_status[si] = CXF_VAR_AT_LOWER;

        /* All <= constraints with rhs=100, max_act=5 => slack=95 (inactive) */
        work_sense[i] = '<';
        work_rhs[i] = 100.0;
        min_act[i] = 0.0;
        max_act[i] = 5.0;
    }

    state.work_lb = work_lb;
    state.work_ub = work_ub;
    state.work_x = work_x;
    state.work_obj = work_obj;
    state.work_dj = work_dj;
    state.work_rhs = work_rhs;
    state.work_sense = work_sense;
    state.min_activity = min_act;
    state.max_activity = max_act;

    state.csr_row_ptr = csr_row_ptr;
    state.csr_col_idx = csr_col_idx;
    state.csr_values = csr_values;
    state.csc_col_ptr = csc_col_ptr;
    state.csc_row_idx = csc_row_idx;
    state.csc_values = csc_values;

    pricing.num_vars = NUM_VARS;
    pricing.var_dirty = var_dirty;
    pricing.num_dirty = 0;
    state.pricing = &pricing;
}

void tearDown(void) {
    free(work_lb);   free(work_ub);   free(work_obj);
    free(work_x);    free(work_dj);   free(work_rhs);
    free(work_sense); free(min_act);  free(max_act);
    free(var_status); free(var_dirty);
    free(csr_row_ptr); free(csr_col_idx); free(csr_values);
    free(csc_row_idx); free(csc_values);
}

/**
 * Core regression test: with 400 inactive constraints, ALL must have
 * their activity bounds recomputed. Old code capped at 256.
 */
void test_all_400_constraints_recomputed(void) {
    /* Seed activity with sentinel values that cxf_compute_activity_bounds
     * will overwrite to 0.0 (then reaccumulate from CSC data). */
    for (int i = 0; i < NUM_CONSTRS; i++) {
        min_act[i] = -999.0;
        max_act[i] = 5.0;  /* inactive: rhs=100, max=5, slack=95 */
    }

    int rc = cxf_simplex_phase_end(&state, &env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* After recomputation, activity bounds reflect CSC data:
     * col 0 has lb=0, ub=10, coeff=1.0 in every row.
     * min_activity[i] = 1.0 * 0.0 = 0.0
     * max_activity[i] = 1.0 * 10.0 = 10.0
     * Check constraints BEYOND the old 256 limit. */
    for (int i = 257; i < NUM_CONSTRS; i++) {
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-10, 0.0, min_act[i],
            "constraint beyond 256: min_activity not recomputed");
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-10, 10.0, max_act[i],
            "constraint beyond 256: max_activity not recomputed");
    }
}

/**
 * Verify constraint 399 (last one) is recomputed correctly.
 */
void test_last_constraint_recomputed(void) {
    min_act[399] = -999.0;  /* sentinel */
    max_act[399] = 5.0;     /* inactive */

    int rc = cxf_simplex_phase_end(&state, &env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, min_act[399]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 10.0, max_act[399]);
}

/**
 * Mixed: first 256 active (tight), rest inactive.
 * Only constraints >= 256 should be recomputed.
 */
void test_mixed_active_inactive_beyond_256(void) {
    /* Make first 256 constraints active (tight): slack = 0 */
    for (int i = 0; i < 256; i++) {
        work_rhs[i] = 5.0;   /* rhs matches max_act => slack=0 */
        max_act[i] = 5.0;
        min_act[i] = -999.0; /* sentinel: should NOT be recomputed */
    }

    /* Constraints 256..399 remain inactive (slack=95) */
    for (int i = 256; i < NUM_CONSTRS; i++) {
        min_act[i] = -999.0; /* sentinel: SHOULD be recomputed */
        max_act[i] = 5.0;
    }

    int rc = cxf_simplex_phase_end(&state, &env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Active constraints: min_act sentinel should be UNTOUCHED */
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-10, -999.0, min_act[i],
            "active constraint should not be recomputed");
    }

    /* Inactive constraints beyond 256: must be recomputed */
    for (int i = 256; i < NUM_CONSTRS; i++) {
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-10, 0.0, min_act[i],
            "inactive constraint beyond 256 was not recomputed");
    }
}

/**
 * Exactly 256 constraints (boundary): all should still work.
 */
void test_exactly_256_constraints(void) {
    state.num_constrs = 256;
    csr_row_ptr[256] = 256;
    csc_col_ptr[1] = 256;

    for (int i = 0; i < 256; i++) {
        min_act[i] = -999.0;
        max_act[i] = 5.0;
    }

    int rc = cxf_simplex_phase_end(&state, &env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* All 256 constraints inactive => all recomputed */
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-10, 0.0, min_act[i],
            "constraint at boundary 256 not recomputed");
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_all_400_constraints_recomputed);
    RUN_TEST(test_last_constraint_recomputed);
    RUN_TEST(test_mixed_active_inactive_beyond_256);
    RUN_TEST(test_exactly_256_constraints);

    return UNITY_END();
}
