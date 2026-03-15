/**
 * @file test_cancellation_detect.c
 * @brief Cancellation detection tests for cxf_pivot_update per
 *        numerical_stability.md Section B.
 *
 * Verifies that after computing result = existing + delta, the code
 * checks (result - delta) != existing. On cancellation, min bounds
 * are rounded upward and max bounds rounded downward.
 *
 * IEEE754 property used: at 2^53 (ULP=2), adding +1.0 absorbs
 * the delta (result = 2^53), but (result-1) = 2^53-1 != 2^53,
 * reliably triggering cancellation detection.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include <string.h>
#include <math.h>

extern void cxf_pivot_update(SolverState *state, int col,
                             double oldLB, double newLB,
                             double oldUB, double newUB,
                             double infinityThreshold);

static SolverState st;
static double min_act[1], max_act[1];
static int neg_unbd[1], pos_unbd[1];
static int64_t col_ptr[2] = {0, 1};
static int row_idx[1]     = {0};
static double vals[1]     = {1.0};
static double wlb[1], wub[1];

void setUp(void) {
    memset(&st, 0, sizeof(st));
    st.num_vars = 1;
    st.num_constrs = 1;
    st.csc_col_ptr = col_ptr;
    st.csc_row_idx = row_idx;
    st.csc_values = vals;
    st.min_activity = min_act;
    st.max_activity = max_act;
    st.negUnbdCount = neg_unbd;
    st.posUnbdCount = pos_unbd;
    st.work_lb = wlb;
    st.work_ub = wub;
    vals[0] = 1.0;
    min_act[0] = 0.0;
    max_act[0] = 0.0;
    neg_unbd[0] = 0;
    pos_unbd[0] = 0;
    wlb[0] = 0.0;
    wub[0] = 100.0;
}

void tearDown(void) {}

/* Finite-to-finite: min cancellation widens upward */
void test_cancel_ff_min(void) {
    double two53 = ldexp(1.0, 53);
    min_act[0] = two53;
    /* a=1, LB: 0->1. delta=+1.0 at ULP=2. Cancel detected. */
    cxf_pivot_update(&st, 0, 0.0, 1.0, 100.0, 100.0, 1e100);
    TEST_ASSERT_TRUE(min_act[0] > two53);
}

/* Finite-to-finite: max cancellation widens downward */
void test_cancel_ff_max(void) {
    double two53 = ldexp(1.0, 53);
    max_act[0] = two53;
    /* a=1, UB: 100->101. delta=+1.0. Cancel detected. */
    cxf_pivot_update(&st, 0, 0.0, 0.0, 100.0, 101.0, 1e100);
    TEST_ASSERT_TRUE(max_act[0] < two53);
}

/* No cancellation: exact arithmetic with small values */
void test_no_cancel_exact(void) {
    min_act[0] = 10.0;
    cxf_pivot_update(&st, 0, 0.0, 3.0, 100.0, 100.0, 1e100);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 13.0, min_act[0]);
}

/* Inf-to-finite on min: safe_add detects cancel */
void test_cancel_itf_min(void) {
    double two53 = ldexp(1.0, 53);
    min_act[0] = two53;
    neg_unbd[0] = 1;
    /* LB: -inf -> 1.0. a=1 positive => min route.
     * safe_add(two53, 1.0*1.0=+1.0). Cancel. */
    cxf_pivot_update(&st, 0, -1e100, 1.0, 100.0, 100.0, 1e100);
    TEST_ASSERT_TRUE(min_act[0] > two53);
    TEST_ASSERT_EQUAL_INT(0, neg_unbd[0]);
}

/* Inf-to-finite on max: safe_add detects cancel */
void test_cancel_itf_max(void) {
    double two53 = ldexp(1.0, 53);
    max_act[0] = two53;
    pos_unbd[0] = 1;
    /* UB: +inf -> 1.0. a=1 positive => max route.
     * safe_add(two53, 1.0*1.0=+1.0). Cancel => widened down. */
    cxf_pivot_update(&st, 0, 0.0, 0.0, 1e100, 1.0, 1e100);
    TEST_ASSERT_TRUE(max_act[0] < two53);
    TEST_ASSERT_EQUAL_INT(0, pos_unbd[0]);
}

/* Finite-to-inf on max: negative coeff produces +delta, cancel */
void test_cancel_fti_max(void) {
    double two53 = ldexp(1.0, 53);
    vals[0] = -1.0;
    max_act[0] = two53;
    /* LB: 1.0 -> -inf. a=-1 negative => max route.
     * delta = -(a*old_val) = -(-1*1) = +1.0. Cancel. */
    cxf_pivot_update(&st, 0, 1.0, -1e100, 100.0, 100.0, 1e100);
    TEST_ASSERT_TRUE(max_act[0] < two53);
    TEST_ASSERT_EQUAL_INT(1, pos_unbd[0]);
    vals[0] = 1.0;
}

/* Negative coefficient: cancel on max via LB change */
void test_cancel_neg_coeff_max(void) {
    double two53 = ldexp(1.0, 53);
    vals[0] = -1.0;
    max_act[0] = two53;
    /* a=-1, LB: 100->99. delta = -1*(99-100) = +1.0 on max.
     * safe_add(two53, +1.0). Cancel => widened down. */
    cxf_pivot_update(&st, 0, 100.0, 99.0, 101.0, 101.0, 1e100);
    TEST_ASSERT_TRUE(max_act[0] < two53);
    vals[0] = 1.0;
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cancel_ff_min);
    RUN_TEST(test_cancel_ff_max);
    RUN_TEST(test_no_cancel_exact);
    RUN_TEST(test_cancel_itf_min);
    RUN_TEST(test_cancel_itf_max);
    RUN_TEST(test_cancel_fti_max);
    RUN_TEST(test_cancel_neg_coeff_max);
    return UNITY_END();
}
