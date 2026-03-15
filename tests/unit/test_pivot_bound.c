/**
 * @file test_pivot_bound.c
 * @brief Tests for cxf_pivot_bound (7-phase variable fixing).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <stdlib.h>

/* Function under test */
int cxf_pivot_bound(void *env, void *state, int var, double new_value,
                    double upperBound, int mode);

/* Minimal test fixtures */
static CxfEnv test_env;
static SolverState test_state;
static BasisState test_basis;

/* Working arrays (3 vars + 2 slacks = 5 total) */
static double work_lb[5], work_ub[5], work_obj[5], work_x[5];
static int var_status[5];
static double min_act[2], max_act[2];
static int neg_unbd[2], pos_unbd[2];

/* CSC for 2x3 matrix: col0=[2.0 in row0], col1=[-1.0 in row1], col2=[3.0 in row0, -2.0 in row1] */
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

    test_basis.n = 5;  /* total = num_vars + num_constrs */
    test_basis.var_status = var_status;
    test_basis.eta_pool = NULL;
    test_basis.eta_head = NULL;
    test_basis.eta_count = 0;

    /* Default bounds: var0=[0,10], var1=[1,5], var2=[-2,8]
     * Slacks: var3=[0,inf], var4=[0,inf] */
    work_lb[0] = 0.0;  work_ub[0] = 10.0;
    work_lb[1] = 1.0;  work_ub[1] = 5.0;
    work_lb[2] = -2.0; work_ub[2] = 8.0;
    work_lb[3] = 0.0;  work_ub[3] = CXF_INFINITY;
    work_lb[4] = 0.0;  work_ub[4] = CXF_INFINITY;

    work_obj[0] = 3.0; work_obj[1] = -2.0; work_obj[2] = 1.5;
    work_obj[3] = 0.0; work_obj[4] = 0.0;
    test_state.obj_value = 0.0;
    test_state.work_lb = work_lb;
    test_state.work_ub = work_ub;
    test_state.work_obj = work_obj;
    test_state.work_x = work_x;

    var_status[0] = CXF_VAR_AT_LOWER;
    var_status[1] = CXF_VAR_AT_UPPER;
    var_status[2] = CXF_VAR_AT_LOWER;
    var_status[3] = CXF_VAR_AT_LOWER;  /* slack 0 */
    var_status[4] = CXF_VAR_AT_LOWER;  /* slack 1 */

    /* Activity arrays */
    min_act[0] = 10.0; min_act[1] = 20.0;
    max_act[0] = 50.0; max_act[1] = 60.0;
    neg_unbd[0] = 0; neg_unbd[1] = 0;
    pos_unbd[0] = 0; pos_unbd[1] = 0;
    test_state.min_activity = min_act;
    test_state.max_activity = max_act;
    test_state.negUnbdCount = neg_unbd;
    test_state.posUnbdCount = pos_unbd;

    /* CSC data */
    test_state.csc_col_ptr = csc_col_ptr;
    test_state.csc_row_idx = csc_row_idx;
    test_state.csc_values = csc_values;

    /* No pricing for unit tests */
    test_state.pricing = NULL;
}

void tearDown(void) {
    /* Free any eta records allocated by calloc during tests */
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
    int rc = cxf_pivot_bound(NULL, &test_state, 0, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_null_state(void) {
    int rc = cxf_pivot_bound(&test_env, NULL, 0, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_invalid_var_negative(void) {
    int rc = cxf_pivot_bound(&test_env, &test_state, -1, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

void test_invalid_var_too_large(void) {
    /* var=5 exceeds num_vars(3) + num_constrs(2) = 5 total */
    int rc = cxf_pivot_bound(&test_env, &test_state, 5, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);
}

void test_slack_var_accepted(void) {
    /* var=3 is first slack (num_vars=3), should be accepted */
    work_obj[0] = 0.0; /* reuse array — extend if needed */
    int rc = cxf_pivot_bound(&test_env, &test_state, 3, 2.0, 5.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*=== Objective update tests ===*/

void test_objective_update(void) {
    /* var0: work_obj=3.0, fix at 5.0 → obj += 3.0*5.0 = 15.0 */
    int rc = cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 15.0, test_state.obj_value);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, test_state.work_obj[0]);
}

/*=== Bounds and status tests ===*/

void test_bounds_set_to_fixed_value(void) {
    cxf_pivot_bound(&test_env, &test_state, 1, 3.0, 5.0, 0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, work_lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, work_ub[1]);
}

void test_status_set_to_fixed(void) {
    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_VAR_FIXED, var_status[0]);
}

/*=== Eta record tests ===*/

void test_eta_record_created(void) {
    TEST_ASSERT_NULL(test_basis.eta_head);
    TEST_ASSERT_EQUAL_INT(0, test_basis.eta_count);

    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);

    TEST_ASSERT_NOT_NULL(test_basis.eta_head);
    TEST_ASSERT_EQUAL_INT(1, test_basis.eta_count);
    TEST_ASSERT_EQUAL_INT(CXF_ETA_VARIABLE_FIX, test_basis.eta_head->type);
    TEST_ASSERT_EQUAL_INT(0, test_basis.eta_head->entering_var);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, test_basis.eta_head->pivot_elem);
}

void test_eta_stores_previous_rc_and_status(void) {
    /* var0: work_obj=3.0, status=AT_LOWER before fix */
    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, test_basis.eta_head->reduced_cost);
    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_LOWER, test_basis.eta_head->status);
}

/*=== Activity bound propagation tests ===*/

void test_activity_positive_coeff(void) {
    /* Fix var0 at 5.0 (old_lb=0, old_ub=upperBound=10.0)
     * col0: coeff=2.0 in row0 (positive)
     * delta_min = 2.0*(5.0 - 0.0) = 10.0 → min_act[0]: 10+10=20
     * delta_max = 2.0*(5.0 - 10.0) = -10.0 → max_act[0]: 50-10=40 */
    cxf_pivot_bound(&test_env, &test_state, 0, 5.0, 10.0, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, min_act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 40.0, max_act[0]);
}

void test_activity_negative_coeff(void) {
    /* Fix var1 at 3.0 (old_lb=1, old_ub=upperBound=5.0)
     * col1: coeff=-1.0 in row1 (negative)
     * delta_min = -1.0*(3.0 - 5.0) = 2.0 → min_act[1]: 20+2=22
     * delta_max = -1.0*(3.0 - 1.0) = -2.0 → max_act[1]: 60-2=58 */
    cxf_pivot_bound(&test_env, &test_state, 1, 3.0, 5.0, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 22.0, min_act[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 58.0, max_act[1]);
}

void test_activity_multi_row(void) {
    /* Fix var2 at 0.0 (old_lb=-2, old_ub=upperBound=8.0)
     * col2: coeff=3.0 in row0 (positive), coeff=-2.0 in row1 (negative)
     * row0: delta_min = 3*(0-(-2))=6, delta_max = 3*(0-8)=-24
     * row1: delta_min = -2*(0-8)=16, delta_max = -2*(0-(-2))=-4 */
    cxf_pivot_bound(&test_env, &test_state, 2, 0.0, 8.0, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 16.0, min_act[0]);  /* 10+6 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 26.0, max_act[0]);  /* 50-24 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 36.0, min_act[1]);  /* 20+16 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 56.0, max_act[1]);  /* 60-4 */
}

/*=== Main ===*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_env);
    RUN_TEST(test_null_state);
    RUN_TEST(test_invalid_var_negative);
    RUN_TEST(test_invalid_var_too_large);
    RUN_TEST(test_slack_var_accepted);
    RUN_TEST(test_objective_update);
    RUN_TEST(test_bounds_set_to_fixed_value);
    RUN_TEST(test_status_set_to_fixed);
    RUN_TEST(test_eta_record_created);
    RUN_TEST(test_eta_stores_previous_rc_and_status);
    RUN_TEST(test_activity_positive_coeff);
    RUN_TEST(test_activity_negative_coeff);
    RUN_TEST(test_activity_multi_row);

    return UNITY_END();
}
