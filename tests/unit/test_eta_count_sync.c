/**
 * @file test_eta_count_sync.c
 * @brief Tests that SolverState.eta_count stays in sync with BasisState.eta_count.
 *
 * V2 spec (solver_state.md): SolverState.etaTotalCount is a convenience alias
 * of BasisState.etaTotalCount and must be kept in sync at all times.
 *
 * Issue: convexfeld-0ev4
 */

#include "unity.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>
#include <stdlib.h>

/* Functions under test */
int cxf_pivot_with_eta(BasisState *basis, int pivotRow, const double *pivotCol,
                       int enteringVar, int leavingVar, int leavingStatus,
                       double reducedCost, int direction);
int cxf_pivot_bound(void *env, void *state, int var, double new_value,
                    double upperBound, int mode);
int cxf_simplex_refine(SolverState *state, CxfEnv *env);
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

/* Minimal fixtures: stack-allocated structs */
static CxfEnv test_env;
static SolverState test_state;

/* Working arrays (2 vars + 2 slacks = 4 total) */
static double work_lb[4], work_ub[4], work_obj[4], work_x[4];
static double min_act[2], max_act[2];
static int neg_unbd[2], pos_unbd[2];

/* CSC for 2x2 identity: col0=[1.0 in row0], col1=[1.0 in row1] */
static int64_t csc_col_ptr[3] = {0, 1, 2};
static int csc_row_idx[2] = {0, 1};
static double csc_values[2] = {1.0, 1.0};

static BasisState *basis = NULL;

void setUp(void) {
    memset(&test_env, 0, sizeof(test_env));
    memset(&test_state, 0, sizeof(test_state));
    test_env.feasibility_tol = 1e-8;
    test_env.optimality_tol = 1e-8;

    basis = cxf_basis_create(2, 4);
    TEST_ASSERT_NOT_NULL(basis);

    /* Slacks basic: row0=var2, row1=var3 */
    basis->basic_vars[0] = 2;
    basis->basic_vars[1] = 3;
    basis->var_status[0] = CXF_VAR_AT_LOWER;
    basis->var_status[1] = CXF_VAR_AT_LOWER;
    basis->var_status[2] = 0;  /* basic in row 0 */
    basis->var_status[3] = 1;  /* basic in row 1 */

    test_state.num_vars = 2;
    test_state.num_constrs = 2;
    test_state.basis = basis;
    test_state.eta_count = 0;

    work_lb[0] = 0.0;  work_ub[0] = 10.0;
    work_lb[1] = 0.0;  work_ub[1] = 10.0;
    work_lb[2] = 0.0;  work_ub[2] = CXF_INFINITY;
    work_lb[3] = 0.0;  work_ub[3] = CXF_INFINITY;
    memset(work_obj, 0, sizeof(work_obj));
    work_obj[0] = 1.0; work_obj[1] = 1.0;
    memset(work_x, 0, sizeof(work_x));

    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;

    min_act[0] = 0.0; min_act[1] = 0.0;
    max_act[0] = 10.0; max_act[1] = 10.0;
    neg_unbd[0] = 0; neg_unbd[1] = 0;
    pos_unbd[0] = 0; pos_unbd[1] = 0;
    test_state.min_activity = min_act;
    test_state.max_activity = max_act;
    test_state.negUnbdCount = neg_unbd;
    test_state.posUnbdCount = pos_unbd;

    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx;
    test_state.csc_values = csc_values;
    test_state.pricing = NULL;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
    test_state.basis = NULL;
}

/*--- Test: both start at zero ---*/

void test_init_both_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, test_state.eta_count);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);
}

/*--- Test: pivot_bound syncs state->eta_count ---*/

void test_pivot_bound_syncs(void) {
    int rc = cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Both must be 1 */
    TEST_ASSERT_EQUAL_INT(1, basis->eta_count);
    TEST_ASSERT_EQUAL_INT(1, test_state.eta_count);
    TEST_ASSERT_EQUAL_INT(test_state.eta_count, basis->eta_count);
}

/*--- Test: multiple pivot_bound calls stay synced ---*/

void test_multiple_pivot_bound_synced(void) {
    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);
    cxf_pivot_bound(&test_env, &test_state, 1, 3.0, 10.0, 0);

    TEST_ASSERT_EQUAL_INT(2, basis->eta_count);
    TEST_ASSERT_EQUAL_INT(2, test_state.eta_count);
    TEST_ASSERT_EQUAL_INT(test_state.eta_count, basis->eta_count);
}

/*--- Test: refine syncs state->eta_count ---*/

void test_refine_syncs(void) {
    /* Set up a nonbasic variable with drift from its bound */
    work_x[0] = 0.001;
    basis->var_status[0] = CXF_VAR_AT_LOWER;

    int rc = cxf_simplex_refine(&test_state, &test_env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* After refine, counts must match */
    TEST_ASSERT_EQUAL_INT(test_state.eta_count, basis->eta_count);
}

/*--- Test: pivot_with_eta + manual sync pattern ---*/

void test_pivot_with_eta_basis_increments(void) {
    double col[] = {2.0, 0.5};
    int rc = cxf_pivot_with_eta(basis, 0, col, 0, 2,
                                CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* BasisState incremented */
    TEST_ASSERT_EQUAL_INT(1, basis->eta_count);

    /* step.c sync pattern */
    test_state.eta_count = basis->eta_count;
    TEST_ASSERT_EQUAL_INT(test_state.eta_count, basis->eta_count);
}

/*--- Test: desync detected without fix ---*/

void test_desync_scenario(void) {
    /* Before the fix in pivot_special.c, pivot_bound would increment
     * basis->eta_count but not state->eta_count. This test verifies
     * both are now synced. */
    TEST_ASSERT_EQUAL_INT(0, test_state.eta_count);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);

    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);

    /* The fix ensures state->eta_count is synced */
    TEST_ASSERT_EQUAL_INT(basis->eta_count, test_state.eta_count);
    TEST_ASSERT_TRUE(test_state.eta_count > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_both_zero);
    RUN_TEST(test_pivot_bound_syncs);
    RUN_TEST(test_multiple_pivot_bound_synced);
    RUN_TEST(test_refine_syncs);
    RUN_TEST(test_pivot_with_eta_basis_increments);
    RUN_TEST(test_desync_scenario);
    return UNITY_END();
}
