/**
 * @file test_midpoint_snap.c
 * @brief V2 spec compliance: fixed variable midpoint snapping.
 *
 * numerical_stability.md Section C: "Should be fixed at the midpoint
 * (lb + ub) / 2 when the gap is nonzero but within tolerance, to
 * minimize the constraint perturbation from the fixing operation."
 *
 * Issue: convexfeld-6e7z.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "midpoint_snap", 0, NULL, NULL, NULL,
                 NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/**
 * Core test: variable with lb=1.0, ub=1.0+5e-11 (gap within
 * CXF_BOUND_EQUALITY_TOL=1e-10) gets fixed at midpoint.
 */
void test_nearly_fixed_var_snaps_to_midpoint(void) {
    double lo = 1.0;
    double hi = lo + 5e-11;  /* gap = 5e-11 < 1e-10 */
    double expected_mid = 0.5 * (lo + hi);

    cxf_addvar(model, 0, NULL, NULL, 0.0, lo, hi, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Both bounds should be set to the midpoint */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, expected_mid, state->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, expected_mid, state->work_ub[0]);

    /* work_x should also be at midpoint */
    if (state->work_x != NULL) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-15, expected_mid,
                                  state->work_x[0]);
    }

    cxf_state_free(state);
}

/**
 * Exactly-fixed variable (gap == 0) stays at its bound, not shifted.
 */
void test_exactly_fixed_var_stays_at_bound(void) {
    double val = 3.0;
    cxf_addvar(model, 0, NULL, NULL, 0.0, val, val, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, val, state->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, val, state->work_ub[0]);

    cxf_state_free(state);
}

/**
 * Variable with gap larger than bound_equality_tol but within tightness
 * threshold (10*feas_tol) should snap to the closest bound, not midpoint.
 */
void test_wider_gap_snaps_to_bound_not_midpoint(void) {
    double lo = 2.0;
    double hi = lo + 1e-7;  /* gap = 1e-7 > 1e-10, < 10*1e-6 */

    cxf_addvar(model, 0, NULL, NULL, 0.0, lo, hi, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Gap exceeds CXF_BOUND_EQUALITY_TOL => closest bound, not midpoint.
     * work_x starts at lb after init, so closest = lb. */
    double target = state->work_lb[0];
    TEST_ASSERT_TRUE(fabs(target - lo) < 1e-12 ||
                     fabs(target - hi) < 1e-12);

    cxf_state_free(state);
}

/**
 * Boundary: gap exactly at CXF_BOUND_EQUALITY_TOL (1e-10) should NOT
 * trigger midpoint snap (condition is range < tol, not <=).
 */
void test_gap_at_tolerance_boundary_no_midpoint(void) {
    double lo = 5.0;
    double hi = lo + CXF_BOUND_EQUALITY_TOL;  /* gap == 1e-10 */

    cxf_addvar(model, 0, NULL, NULL, 0.0, lo, hi, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* gap == tol: not strictly less, so closest-bound path */
    double mid = 0.5 * (lo + hi);
    double target = state->work_lb[0];
    /* Should be at lo or hi, not the midpoint */
    TEST_ASSERT_TRUE(fabs(target - lo) < 1e-12 ||
                     fabs(target - hi) < 1e-12);
    (void)mid;

    cxf_state_free(state);
}

/**
 * Multiple variables: only the nearly-fixed one gets midpoint snap.
 */
void test_mixed_vars_only_nearly_fixed_snaps(void) {
    /* var 0: wide bounds, no snap */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "wide");
    /* var 1: nearly fixed, should snap to midpoint */
    double lo = 7.0, hi = lo + 3e-11;
    cxf_addvar(model, 0, NULL, NULL, 0.0, lo, hi, 'C', "tight");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* var 0: wide bounds untouched */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, state->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, state->work_ub[0]);

    /* var 1: nearly fixed => midpoint */
    double expected = 0.5 * (lo + hi);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, expected, state->work_lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, expected, state->work_ub[1]);

    cxf_state_free(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nearly_fixed_var_snaps_to_midpoint);
    RUN_TEST(test_exactly_fixed_var_stays_at_bound);
    RUN_TEST(test_wider_gap_snaps_to_bound_not_midpoint);
    RUN_TEST(test_gap_at_tolerance_boundary_no_midpoint);
    RUN_TEST(test_mixed_vars_only_nearly_fixed_snaps);
    return UNITY_END();
}
