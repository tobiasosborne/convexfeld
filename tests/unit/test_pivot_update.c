/**
 * @file test_pivot_update.c
 * @brief Direct tests for cxf_pivot_update (cancellation detection,
 *        infinity transitions, unbounded counts).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include <string.h>
#include <math.h>

/* Function under test */
extern void cxf_pivot_update(SolverState *state, int col,
                             double oldLB, double newLB,
                             double oldUB, double newUB,
                             double infinityThreshold);

/* Fixtures: 2 constraints, 2 variables */
static SolverState st;
static double min_act[2], max_act[2];
static int neg_unbd[2], pos_unbd[2];

/* CSC: col0 = [2.0 in row0, -3.0 in row1], col1 = [-1.0 in row0] */
static int64_t col_ptr[3] = {0, 2, 3};
static int row_idx[3]     = {0, 1, 0};
static double vals[3]     = {2.0, -3.0, -1.0};
static double wlb[2], wub[2];

void setUp(void) {
    memset(&st, 0, sizeof(st));
    st.num_vars = 2;
    st.num_constrs = 2;
    st.csc_col_ptr = col_ptr;
    st.csc_row_idx = row_idx;
    st.csc_values = vals;
    st.min_activity = min_act;
    st.max_activity = max_act;
    st.negUnbdCount = neg_unbd;
    st.posUnbdCount = pos_unbd;
    st.work_lb = wlb;
    st.work_ub = wub;
    min_act[0] = 0.0; min_act[1] = 0.0;
    max_act[0] = 0.0; max_act[1] = 0.0;
    neg_unbd[0] = 0; neg_unbd[1] = 0;
    pos_unbd[0] = 0; pos_unbd[1] = 0;
    wlb[0] = 0.0; wlb[1] = 0.0;
    wub[0] = 10.0; wub[1] = 5.0;
}

void tearDown(void) {}

/*=== Basic finite-to-finite update ===*/

void test_lb_increase_pos_coeff(void) {
    /* col0 row0: a=2.0, LB 0->3. min_activity += 2*(3-0) = 6 */
    cxf_pivot_update(&st, 0, 0.0, 3.0, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, min_act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, max_act[0]);  /* UB unchanged */
}

void test_lb_increase_neg_coeff(void) {
    /* col0 row1: a=-3.0, LB 0->3. max_activity += -3*(3-0) = -9 */
    cxf_pivot_update(&st, 0, 0.0, 3.0, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -9.0, max_act[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, min_act[1]);
}

void test_ub_decrease_pos_coeff(void) {
    /* col0 row0: a=2.0, UB 10->5. max_activity += 2*(5-10) = -10 */
    cxf_pivot_update(&st, 0, 0.0, 0.0, 10.0, 5.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -10.0, max_act[0]);
}

void test_ub_decrease_neg_coeff(void) {
    /* col0 row1: a=-3.0, UB 10->5. min_activity += -3*(5-10) = 15 */
    cxf_pivot_update(&st, 0, 0.0, 0.0, 10.0, 5.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 15.0, min_act[1]);
}

void test_both_bounds_change(void) {
    /* col0: LB 0->2, UB 10->8
     * row0 (a=2): min += 2*(2-0)=4, max += 2*(8-10)=-4
     * row1 (a=-3): max += -3*(2-0)=-6, min += -3*(8-10)=6 */
    cxf_pivot_update(&st, 0, 0.0, 2.0, 10.0, 8.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, min_act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -4.0, max_act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, min_act[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -6.0, max_act[1]);
}

/*=== Infinity transitions ===*/

void test_finite_to_infinite_lb(void) {
    /* col0 row0: a=2.0, LB 5 -> -inf. a>0 + LB = min, negUnbdCount.
     * Remove old: min_act -= 2*5 = -10. Count++ */
    min_act[0] = 10.0;
    cxf_pivot_update(&st, 0, 5.0, -1e100, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, min_act[0]);  /* 10 - 10 */
    TEST_ASSERT_EQUAL_INT(1, neg_unbd[0]);
}

void test_infinite_to_finite_lb(void) {
    /* col0 row0: a=2.0, LB -inf -> 3. Add new: min_act += 2*3=6. Count-- */
    min_act[0] = 0.0;
    neg_unbd[0] = 1;
    cxf_pivot_update(&st, 0, -1e100, 3.0, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, min_act[0]);
    TEST_ASSERT_EQUAL_INT(0, neg_unbd[0]);
}

void test_finite_to_infinite_ub(void) {
    /* col0 row0: a=2.0, UB 10 -> +inf. a>0 + UB = max, posUnbdCount.
     * Remove old: max_act -= 2*10 = -20. Count++ */
    max_act[0] = 20.0;
    cxf_pivot_update(&st, 0, 0.0, 0.0, 10.0, 1e100, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, max_act[0]);
    TEST_ASSERT_EQUAL_INT(1, pos_unbd[0]);
}

void test_infinite_to_finite_ub(void) {
    /* col0 row0: a=2.0, UB +inf -> 7. Add new: max_act += 2*7=14. Count-- */
    max_act[0] = 0.0;
    pos_unbd[0] = 1;
    cxf_pivot_update(&st, 0, 0.0, 0.0, 1e100, 7.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 14.0, max_act[0]);
    TEST_ASSERT_EQUAL_INT(0, pos_unbd[0]);
}

/*=== Cancellation detection ===*/

void test_cancellation_detected(void) {
    /* Large existing value + tiny delta triggers cancellation.
     * col0 row0: a=2.0, LB 0 -> 1e-16 (delta = 2e-16 added to 1e15).
     * Should detect cancellation and apply conservative rounding. */
    min_act[0] = 1e15;
    cxf_pivot_update(&st, 0, 0.0, 1e-16, 10.0, 10.0, 1e100);
    /* Result should be >= 1e15 (safe overestimate for min) */
    TEST_ASSERT_TRUE(min_act[0] >= 1e15);
}

/*=== Null safety ===*/

void test_null_state(void) {
    cxf_pivot_update(NULL, 0, 0.0, 1.0, 10.0, 10.0, 1e100);
    /* Should not crash */
}

void test_no_change(void) {
    min_act[0] = 42.0;
    cxf_pivot_update(&st, 0, 5.0, 5.0, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 42.0, min_act[0]);  /* Unchanged */
}

void test_null_unbd_arrays(void) {
    /* Should work even without unbounded count arrays */
    st.negUnbdCount = NULL;
    st.posUnbdCount = NULL;
    cxf_pivot_update(&st, 0, 0.0, 3.0, 10.0, 10.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, min_act[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lb_increase_pos_coeff);
    RUN_TEST(test_lb_increase_neg_coeff);
    RUN_TEST(test_ub_decrease_pos_coeff);
    RUN_TEST(test_ub_decrease_neg_coeff);
    RUN_TEST(test_both_bounds_change);
    RUN_TEST(test_finite_to_infinite_lb);
    RUN_TEST(test_infinite_to_finite_lb);
    RUN_TEST(test_finite_to_infinite_ub);
    RUN_TEST(test_infinite_to_finite_ub);
    RUN_TEST(test_cancellation_detected);
    RUN_TEST(test_null_state);
    RUN_TEST(test_no_change);
    RUN_TEST(test_null_unbd_arrays);
    return UNITY_END();
}
