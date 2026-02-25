/**
 * @file test_phase_end.c
 * @brief Tests for cxf_simplex_phase_end — constraint cleanup + transition.
 *
 * Verifies two_phase_method.md Transition Step 4: inactive constraints
 * (whose activity bounds show they are not binding) are identified.
 *
 * Activity bounds represent a^T x - b (RHS subtracted per simplex_phases.md).
 * For <= constraints: slack = -max_activity (positive when loose).
 * For >= constraints: slack =  min_activity (positive when loose).
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

/* Minimal test fixtures: 3 vars, 3 constraints */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;
static PricingState test_pricing;

static double work_lb[6], work_ub[6], work_obj[6], work_x[6], work_dj[6];
static double work_rhs[3];
static char work_sense[3];
static int var_status[6];
static double min_act[3], max_act[3];
static int basic_vars[3];
static int var_dirty[3];

/* CSR for 3x3 identity-like matrix */
static int64_t csr_row_ptr[4] = {0, 1, 2, 3};
static int csr_col_idx[3] = {0, 1, 2};
static double csr_values[3] = {1.0, 1.0, 1.0};

/* CSC for 3x3 identity-like matrix */
static int64_t csc_col_ptr[4] = {0, 1, 2, 3};
static int csc_row_idx[3] = {0, 1, 2};
static double csc_values[3] = {1.0, 1.0, 1.0};

static int count_dirty(void) {
    int count = 0;
    for (int j = 0; j < 3; j++)
        if (var_dirty[j]) count++;
    return count;
}

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    memset(&test_basis, 0, sizeof(test_basis));
    memset(&test_pricing, 0, sizeof(test_pricing));

    test_env.feasibility_tol = 1e-6;
    test_env.optimality_tol = 1e-6;

    test_state.num_vars = 3;
    test_state.num_constrs = 3;
    test_state.basis = &test_basis;
    test_state.phase = 2;

    test_basis.n = 6;
    test_basis.var_status = var_status;

    for (int j = 0; j < 3; j++) {
        work_lb[j] = 0.0;
        work_ub[j] = 10.0;
        work_x[j] = 0.0;
        work_obj[j] = 1.0;
        work_dj[j] = 0.0;
        var_status[j] = CXF_VAR_AT_LOWER;
        var_dirty[j] = 0;
    }

    for (int i = 0; i < 3; i++) {
        int si = 3 + i;
        work_lb[si] = 0.0;
        work_ub[si] = CXF_INFINITY;
        work_x[si] = 0.0;
        work_dj[si] = 0.0;
        var_status[si] = CXF_VAR_AT_LOWER;
        basic_vars[i] = si;
    }

    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_x = work_x;
    test_state.work_obj = work_obj;
    test_state.work_dj = work_dj;
    test_state.work_rhs = work_rhs;
    test_state.work_sense = work_sense;
    test_state.min_activity = min_act;
    test_state.max_activity = max_act;
    test_basis.basic_vars = basic_vars;

    test_state.csr_row_ptr = csr_row_ptr;
    test_state.csr_col_idx = csr_col_idx;
    test_state.csr_values = csr_values;
    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx;
    test_state.csc_values = csc_values;

    test_pricing.num_vars = 3;
    test_pricing.var_dirty = var_dirty;
    test_pricing.num_dirty = 0;
    test_state.pricing = &test_pricing;

    /* Default: all <= with rhs=10.
     * Activity = a^T x - rhs. With vars in [0,10] and coeff=1:
     *   min_activity = 0 - 10 = -10 (all vars at lb)
     *   max_activity = 10 - 10 = 0   (all vars at ub, exactly tight) */
    work_sense[0] = '<';
    work_sense[1] = '<';
    work_sense[2] = '<';
    work_rhs[0] = 10.0;
    work_rhs[1] = 10.0;
    work_rhs[2] = 10.0;

    min_act[0] = -10.0;  max_act[0] = 0.0;
    min_act[1] = -10.0;  max_act[1] = 0.0;
    min_act[2] = -10.0;  max_act[2] = 0.0;
}

void tearDown(void) {}

/*=== Null argument tests ===*/

void test_null_state(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_phase_end(NULL, &test_env, 0));
}

void test_null_env(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_phase_end(&test_state, NULL, 0));
}

/*=== Inactive constraint detection ===*/

void test_leq_inactive_large_slack(void) {
    /* <= : rhs=100, max(a^T x)=10 → activity = 10-100 = -90.
     * slack = -(-90) = 90 >> threshold. Inactive. */
    max_act[0] = -90.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_TRUE(var_dirty[0]);
}

void test_leq_active_tight(void) {
    /* <= : rhs=10, max(a^T x)=10 → activity = 0. Tight. */
    max_act[0] = 0.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_leq_active_near_tight(void) {
    /* <= : activity = -1e-8. slack = 1e-8 < 10*feas_tol. Still active. */
    max_act[0] = -1e-8;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_geq_inactive_large_slack(void) {
    /* >= : rhs=2, min(a^T x)=50 → activity = 50-2 = 48.
     * slack = 48 >> threshold. Inactive. */
    work_sense[1] = '>';
    min_act[1] = 48.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_TRUE(var_dirty[1]);
}

void test_geq_active_tight(void) {
    /* >= : rhs=5, min(a^T x)=5 → activity = 0. Tight. */
    work_sense[1] = '>';
    min_act[1] = 0.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_equality_never_inactive(void) {
    /* = : always active. Activity = 0 → skip anyway. */
    work_sense[0] = '=';
    min_act[0] = 0.0;
    max_act[0] = 0.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_infinite_activity_skipped(void) {
    min_act[0] = -CXF_INFINITY;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_basic_slack_constraint_skipped(void) {
    /* Large slack but slack var is basic → constraint active, skip. */
    max_act[0] = -90.0;     /* Would be inactive... */
    var_status[3] = 0;      /* ...but slack is basic */

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_FALSE(var_dirty[0]);
}

void test_mixed_senses_only_inactive_flagged(void) {
    /* <= inactive, >= active, = always-active. */
    work_sense[0] = '<'; max_act[0] = -95.0;   /* slack=95 */
    work_sense[1] = '>'; min_act[1] = 0.0;     /* slack=0  */
    work_sense[2] = '='; min_act[2] = 0.0;     /* equality */

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_TRUE(var_dirty[0]);
    TEST_ASSERT_FALSE(var_dirty[1]);
    TEST_ASSERT_FALSE(var_dirty[2]);
}

/*=== Free variable dual infeasibility ===*/

void test_free_var_dual_infeasible(void) {
    var_status[0] = CXF_VAR_SUPERBASIC;
    work_dj[0] = 1.0;

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE,
                          cxf_simplex_phase_end(&test_state, &test_env, 0));
}

void test_free_var_small_rc_ok(void) {
    var_status[0] = CXF_VAR_SUPERBASIC;
    work_dj[0] = 1e-8;

    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_simplex_phase_end(&test_state, &test_env, 0));
}

/*=== Runner ===*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_state);
    RUN_TEST(test_null_env);

    RUN_TEST(test_leq_inactive_large_slack);
    RUN_TEST(test_leq_active_tight);
    RUN_TEST(test_leq_active_near_tight);
    RUN_TEST(test_geq_inactive_large_slack);
    RUN_TEST(test_geq_active_tight);
    RUN_TEST(test_equality_never_inactive);
    RUN_TEST(test_infinite_activity_skipped);
    RUN_TEST(test_basic_slack_constraint_skipped);
    RUN_TEST(test_mixed_senses_only_inactive_flagged);
    RUN_TEST(test_free_var_dual_infeasible);
    RUN_TEST(test_free_var_small_rc_ok);

    return UNITY_END();
}
