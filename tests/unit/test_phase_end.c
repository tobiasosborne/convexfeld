/**
 * @file test_phase_end.c
 * @brief Tests for cxf_simplex_phase_end — constraint cleanup + transition.
 *
 * Verifies two_phase_method.md Transition Step 4: inactive constraints
 * (whose activity bounds show they are not binding) are identified using
 * RHS-aware slack computation for all constraint senses.
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
static int var_dirty[3];  /* Pricing dirty flags for observability */

/* CSR for 3x3 identity-like matrix (for doScan path) */
static int64_t csr_row_ptr[4] = {0, 1, 2, 3};
static int csr_col_idx[3] = {0, 1, 2};
static double csr_values[3] = {1.0, 1.0, 1.0};

/* CSC for 3x3 identity-like matrix */
static int64_t csc_col_ptr[4] = {0, 1, 2, 3};
static int csc_row_idx[3] = {0, 1, 2};
static double csc_values[3] = {1.0, 1.0, 1.0};

/**
 * Count how many variables are marked dirty in the pricing state.
 */
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

    /* Structurals: nonbasic at lower bound */
    for (int j = 0; j < 3; j++) {
        work_lb[j] = 0.0;
        work_ub[j] = 10.0;
        work_x[j] = 0.0;
        work_obj[j] = 1.0;
        work_dj[j] = 0.0;
        var_status[j] = CXF_VAR_AT_LOWER;
        var_dirty[j] = 0;
    }

    /* Slacks: nonbasic at lower bound (default) */
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

    /* Minimal pricing state for dirty-marking observability */
    test_pricing.num_vars = 3;
    test_pricing.var_dirty = var_dirty;
    test_pricing.num_dirty = 0;
    test_state.pricing = &test_pricing;

    /* Default: all constraints <= with rhs=10 */
    work_sense[0] = '<';
    work_sense[1] = '<';
    work_sense[2] = '<';
    work_rhs[0] = 10.0;
    work_rhs[1] = 10.0;
    work_rhs[2] = 10.0;

    /* Default activity: max(a^T x) = 10 (tight), min(a^T x) = 0 */
    min_act[0] = 0.0;  max_act[0] = 10.0;
    min_act[1] = 0.0;  max_act[1] = 10.0;
    min_act[2] = 0.0;  max_act[2] = 10.0;
}

void tearDown(void) {}

/*=== Null argument tests ===*/

void test_null_state(void) {
    int rc = cxf_simplex_phase_end(NULL, &test_env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_null_env(void) {
    int rc = cxf_simplex_phase_end(&test_state, NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/*=== Inactive constraint detection ===*/

void test_leq_inactive_large_slack(void) {
    /* <= constraint: rhs=100, max_activity=10 → slack=90 >> threshold.
     * Variable in constraint 0 (col 0) should be marked dirty. */
    work_rhs[0] = 100.0;
    max_act[0] = 10.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* Constraint 0 is inactive → col 0 marked dirty */
    TEST_ASSERT_TRUE(var_dirty[0]);
}

void test_leq_active_tight(void) {
    /* <= constraint: rhs=10, max_activity=10 → slack=0. Tight. */
    work_rhs[0] = 10.0;
    max_act[0] = 10.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* No constraint should be flagged — no dirty marking */
    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_leq_active_near_tight(void) {
    /* <= constraint: rhs=10, max_activity=9.9999999 → slack < threshold */
    work_rhs[0] = 10.0;
    max_act[0] = 10.0 - 1e-8;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* Slack is below 10*feas_tol → should NOT be flagged */
    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_geq_inactive_large_slack(void) {
    /* >= constraint: rhs=2, min_activity=50 → slack=48 >> threshold */
    work_sense[1] = '>';
    work_rhs[1] = 2.0;
    min_act[1] = 50.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* Constraint 1 is inactive → col 1 marked dirty */
    TEST_ASSERT_TRUE(var_dirty[1]);
}

void test_geq_active_tight(void) {
    /* >= constraint: rhs=5, min_activity=5 → slack=0. Tight. */
    work_sense[1] = '>';
    work_rhs[1] = 5.0;
    min_act[1] = 5.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_equality_never_inactive(void) {
    /* = constraint: always active regardless of activity bounds */
    work_sense[0] = '=';
    work_rhs[0] = 5.0;
    min_act[0] = 5.0;
    max_act[0] = 5.0;  /* Even if perfectly matching */

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_infinite_activity_skipped(void) {
    /* Infinite activity bound → skip (can't determine slack) */
    min_act[0] = -CXF_INFINITY;
    work_rhs[0] = 100.0;

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    TEST_ASSERT_EQUAL_INT(0, count_dirty());
}

void test_basic_slack_constraint_skipped(void) {
    /* Slack variable is basic → constraint is in the basis, skip */
    work_rhs[0] = 100.0;
    max_act[0] = 10.0;  /* Would be inactive... */
    var_status[3] = 0;  /* ...but slack is basic (row 0) */

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* Constraint 0 skipped despite large slack (basic slack) */
    TEST_ASSERT_FALSE(var_dirty[0]);
}

void test_mixed_senses_only_inactive_flagged(void) {
    /* Three constraints: <=inactive, >=active, =always-active.
     * Only constraint 0 should be flagged. */
    work_sense[0] = '<'; work_rhs[0] = 100.0; max_act[0] = 5.0;  /* slack=95 */
    work_sense[1] = '>'; work_rhs[1] = 50.0;  min_act[1] = 50.0; /* slack=0  */
    work_sense[2] = '='; work_rhs[2] = 5.0;   min_act[2] = 5.0;  /* equality */

    cxf_simplex_phase_end(&test_state, &test_env, 0);

    /* Only constraint 0 (col 0) dirty, others clean */
    TEST_ASSERT_TRUE(var_dirty[0]);
    TEST_ASSERT_FALSE(var_dirty[1]);
    TEST_ASSERT_FALSE(var_dirty[2]);
}

/*=== Free variable dual infeasibility ===*/

void test_free_var_dual_infeasible(void) {
    /* Free variable with significant reduced cost → INFEASIBLE */
    var_status[0] = CXF_VAR_SUPERBASIC;
    work_dj[0] = 1.0;  /* Well above optimality tolerance */

    int rc = cxf_simplex_phase_end(&test_state, &test_env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
}

void test_free_var_small_rc_ok(void) {
    /* Free variable with tiny reduced cost → not infeasible */
    var_status[0] = CXF_VAR_SUPERBASIC;
    work_dj[0] = 1e-8;  /* Below optimality tolerance */

    int rc = cxf_simplex_phase_end(&test_state, &test_env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
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
