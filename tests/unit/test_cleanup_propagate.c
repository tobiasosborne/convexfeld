/**
 * @file test_cleanup_propagate.c
 * @brief Tests for iterative 2-pass cleanup propagation (T2.8).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Functions under test */
int cxf_cleanup_propagate(SolverState *state, CxfEnv *env);
double cxf_kahan_activity_sum(double S, double delta, int is_max);
void cxf_kahan_row_activity(
    int64_t, int64_t, const int *, const double *,
    const double *, const double *,
    double *, double *, int *, int *, int);

void setUp(void) {}
void tearDown(void) {}

/*--- Kahan summation ---*/

void test_kahan_no_cancel(void) {
    double r = cxf_kahan_activity_sum(100.0, 1.0, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 101.0, r);
}

void test_kahan_max_rounds_up(void) {
    double r = cxf_kahan_activity_sum(1e15, 1e-3, 1);
    TEST_ASSERT_TRUE(r >= 1e15 + 1e-3);
}

void test_kahan_min_rounds_down(void) {
    double r = cxf_kahan_activity_sum(1e15, 1e-3, 0);
    TEST_ASSERT_TRUE(r <= 1e15 + 1.0);
}

/*--- Row activity ---*/

void test_kahan_row_bounded(void) {
    /* row: 2*x0 - 3*x1, both in [0,10] */
    int ci[2] = {0, 1};
    double cv[2] = {2.0, -3.0};
    double lb[2] = {0, 0}, ub[2] = {10, 10};
    double rmin, rmax; int nc, pc;
    cxf_kahan_row_activity(0, 2, ci, cv, lb, ub,
                           &rmin, &rmax, &nc, &pc, 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -30.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10,  20.0, rmax);
    TEST_ASSERT_EQUAL_INT(0, nc);
    TEST_ASSERT_EQUAL_INT(0, pc);
}

void test_kahan_row_infinite(void) {
    int ci[2] = {0, 1};
    double cv[2] = {2.0, -3.0};
    double lb[2] = {-CXF_INFINITY, 0};
    double ub[2] = {10, CXF_INFINITY};
    double rmin, rmax; int nc, pc;
    cxf_kahan_row_activity(0, 2, ci, cv, lb, ub,
                           &rmin, &rmax, &nc, &pc, 2);
    /* x0 lb=-inf, a>0 => negUnbd; x1 ub=+inf, a<0 => negUnbd */
    TEST_ASSERT_EQUAL_INT(2, nc);
    TEST_ASSERT_EQUAL_INT(0, pc);
}

void test_kahan_null_counts(void) {
    int ci[1] = {0};
    double cv[1] = {1.0};
    double lb[1] = {0}, ub[1] = {5};
    double rmin, rmax;
    /* NULL out_neg_c and out_pos_c should not crash */
    cxf_kahan_row_activity(0, 1, ci, cv, lb, ub,
                           &rmin, &rmax, NULL, NULL, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, rmax);
}

/*--- Propagation: null safety ---*/

void test_propagate_null_state(void) {
    CxfEnv env = {0};
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_cleanup_propagate(NULL, &env));
}

void test_propagate_null_env(void) {
    SolverState st = {0};
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_cleanup_propagate(&st, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_kahan_no_cancel);
    RUN_TEST(test_kahan_max_rounds_up);
    RUN_TEST(test_kahan_min_rounds_down);
    RUN_TEST(test_kahan_row_bounded);
    RUN_TEST(test_kahan_row_infinite);
    RUN_TEST(test_kahan_null_counts);
    RUN_TEST(test_propagate_null_state);
    RUN_TEST(test_propagate_null_env);
    return UNITY_END();
}
