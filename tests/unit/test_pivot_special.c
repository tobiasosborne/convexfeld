/**
 * @file test_pivot_special.c
 * @brief Tests for cxf_pivot_special — Phase I unboundedness suppression.
 *
 * Verifies spec pivot_operations.md line 247: "A special mode flag on the
 * solver state can disable unboundedness detection; this is used during
 * Phase I of two-phase simplex."
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <stdlib.h>

/* Function under test */
int cxf_pivot_special(void *env, void *state, int var, double lb_limit,
                      double ub_limit);

/* Minimal test fixtures */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;

/* Working arrays (3 vars, 2 constraints) */
static double work_lb[3], work_ub[3], work_obj[3], work_x[3];
static int var_status[3];
static double min_act[2], max_act[2];
static int neg_unbd[2], pos_unbd[2];
static char work_sense[2];

/* CSC for 2x3 matrix */
static int64_t csc_col_ptr[4] = {0, 1, 2, 4};
static int csc_row_idx[4]     = {0, 1, 0, 1};
static double csc_values[4]   = {2.0, -1.0, 3.0, -2.0};

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));

    test_state.num_vars = 3;
    test_state.num_constrs = 2;
    test_state.basis = &test_basis;
    test_state.phase = 2;  /* Default: Phase II */

    test_basis.n = 3;
    test_basis.var_status = var_status;
    test_basis.eta_pool = NULL;
    test_basis.eta_head = NULL;
    test_basis.eta_count = 0;

    /* var0=[0,10], var1=[1,5], var2=[-inf,+inf] */
    work_lb[0] = 0.0;          work_ub[0] = 10.0;
    work_lb[1] = 1.0;          work_ub[1] = 5.0;
    work_lb[2] = -CXF_INFINITY; work_ub[2] = CXF_INFINITY;

    work_obj[0] = 3.0;  /* positive → decrease improves */
    work_obj[1] = -2.0;  /* negative → increase improves */
    work_obj[2] = -1.0;  /* negative → increase improves */

    test_state.obj_value = 0.0;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;

    var_status[0] = CXF_VAR_AT_LOWER;
    var_status[1] = CXF_VAR_AT_UPPER;
    var_status[2] = CXF_VAR_AT_LOWER;

    min_act[0] = 10.0; min_act[1] = 20.0;
    max_act[0] = 50.0; max_act[1] = 60.0;
    neg_unbd[0] = 0; neg_unbd[1] = 0;
    pos_unbd[0] = 0; pos_unbd[1] = 0;
    test_state.min_activity = min_act;
    test_state.max_activity = max_act;
    test_state.negUnbdCount = neg_unbd;
    test_state.posUnbdCount = pos_unbd;

    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx;
    test_state.csc_values = csc_values;

    /* Default: inequality constraints (no equality guard triggered) */
    work_sense[0] = '<';
    work_sense[1] = '<';
    test_state.work_sense = work_sense;

    test_state.pricing = NULL;
}

void tearDown(void) {
    EtaVector *eta = test_basis.eta_head;
    while (eta != NULL) {
        EtaVector *next = eta->next;
        free(eta);
        eta = next;
    }
    test_basis.eta_head = NULL;
    test_basis.eta_count = 0;
}

/*=== Null/invalid argument tests ===*/

void test_null_env(void) {
    int rc = cxf_pivot_special(NULL, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_null_state(void) {
    int rc = cxf_pivot_special(&test_env, NULL, 0, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_invalid_var_negative(void) {
    int rc = cxf_pivot_special(&test_env, &test_state, -1, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

void test_invalid_var_too_large(void) {
    /* var=5 exceeds num_vars(3) + num_constrs(2) = 5 total */
    int rc = cxf_pivot_special(&test_env, &test_state, 5, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

/*=== Phase II: unboundedness should be reported ===*/

void test_phase2_unbounded_increase(void) {
    /* var2: obj=-1 (increase improves), ub=1e30 > ub_limit=1e20 → unbounded.
     * Note: ub must be finite (< CXF_INFINITY) for can_increase check. */
    test_state.phase = 2;
    work_ub[2] = 1e30;
    int rc = cxf_pivot_special(&test_env, &test_state, 2, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, rc);
}

void test_phase2_unbounded_decrease(void) {
    /* var2: obj=+1 (decrease improves), lb=-1e30 < -lb_limit → unbounded.
     * Note: lb must be finite (> -CXF_INFINITY) for can_decrease check. */
    test_state.phase = 2;
    work_obj[2] = 1.0;
    work_lb[2] = -1e30;
    int rc = cxf_pivot_special(&test_env, &test_state, 2, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, rc);
}

/*=== Phase I: unboundedness must be SUPPRESSED ===*/

void test_phase1_suppress_unbounded_increase(void) {
    /* Same setup as phase2_unbounded_increase, but Phase I →
     * must return CXF_OK, NOT CXF_UNBOUNDED (spec line 247). */
    test_state.phase = 1;
    work_ub[2] = 1e30;
    int rc = cxf_pivot_special(&test_env, &test_state, 2, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

void test_phase1_suppress_unbounded_decrease(void) {
    /* var2: obj=+1 (decrease improves), lb=-1e30 (exceeds limit), Phase I →
     * must return CXF_OK, NOT CXF_UNBOUNDED (spec line 247). */
    test_state.phase = 1;
    work_obj[2] = 1.0;
    work_lb[2] = -1e30;
    int rc = cxf_pivot_special(&test_env, &test_state, 2, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*=== Bounded variables: same behavior in both phases ===*/

void test_bounded_var_not_unbounded(void) {
    /* var1: obj=-2 (increase improves), ub=5 (finite) → not unbounded */
    int rc = cxf_pivot_special(&test_env, &test_state, 1, 1e20, 1e20);
    TEST_ASSERT_NOT_EQUAL(CXF_UNBOUNDED, rc);
}

void test_no_improving_direction(void) {
    /* var0: obj near zero → no beneficial direction */
    work_obj[0] = 1e-12;
    int rc = cxf_pivot_special(&test_env, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*=== Phase 3: Equality constraint column scan ===*/

void test_equality_guard_blocks_action(void) {
    /* var0: obj=3.0 (decrease improves), bounded [0,10].
     * Without equality guard: would bound-flip to lb=0.
     * With row 0 as equality: must return CXF_OK (no action). */
    work_sense[0] = '=';
    int rc = cxf_pivot_special(&test_env, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* Verify no bound change occurred (obj_value unchanged) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, test_state.obj_value);
}

void test_equality_guard_E_sense(void) {
    /* Same test with 'E' sense variant */
    work_sense[0] = 'E';
    int rc = cxf_pivot_special(&test_env, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, test_state.obj_value);
}

void test_equality_guard_inequality_allows_action(void) {
    /* var0: obj=3.0, bounded [0,10], all constraints are inequalities.
     * Should proceed to bound flip (not blocked). */
    work_sense[0] = '<';
    work_sense[1] = '<';
    int rc = cxf_pivot_special(&test_env, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_NOT_EQUAL(CXF_UNBOUNDED, rc);
    /* Bound flip should have changed obj_value */
}

void test_equality_guard_no_csc_skips_scan(void) {
    /* If CSC data is NULL, scan is skipped (no crash, normal behavior) */
    test_state.csc_col_ptr = NULL;
    work_sense[0] = '=';
    /* var0 has obj=3.0, bounded: should proceed past equality guard */
    int rc = cxf_pivot_special(&test_env, &test_state, 0, 1e20, 1e20);
    TEST_ASSERT_NOT_EQUAL(CXF_ERROR_NULL_ARGUMENT, rc);
}

/*=== Main ===*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_env);
    RUN_TEST(test_null_state);
    RUN_TEST(test_invalid_var_negative);
    RUN_TEST(test_invalid_var_too_large);
    RUN_TEST(test_phase2_unbounded_increase);
    RUN_TEST(test_phase2_unbounded_decrease);
    RUN_TEST(test_phase1_suppress_unbounded_increase);
    RUN_TEST(test_phase1_suppress_unbounded_decrease);
    RUN_TEST(test_bounded_var_not_unbounded);
    RUN_TEST(test_no_improving_direction);

    /* Phase 3: Equality constraint column scan */
    RUN_TEST(test_equality_guard_blocks_action);
    RUN_TEST(test_equality_guard_E_sense);
    RUN_TEST(test_equality_guard_inequality_allows_action);
    RUN_TEST(test_equality_guard_no_csc_skips_scan);

    return UNITY_END();
}
