/**
 * @file test_basis_progress.c
 * @brief Tests for progress snapshot/diff and BasisSnapshot
 *        equality/free operations (split from test_basis.c).
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* BasisState lifecycle */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

/* V2 progress snapshot/diff — external snapshot buffer */
void cxf_progress_snapshot(SolverState *state, int *snapshot);
double cxf_basis_diff(SolverState *state, const int *snapshot);

/* Full BasisSnapshot API (heavy, implementation extension) */
int cxf_basis_snapshot_full(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);
int cxf_basis_snapshot_full_equal(const BasisSnapshot *s1, const BasisSnapshot *s2);
void cxf_basis_snapshot_full_free(BasisSnapshot *snapshot);

void setUp(void) {}
void tearDown(void) {}

/*--- Progress snapshot/comparison tests ---*/

void test_basis_snapshot_captures_counters(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 42;
    state.rows_eliminated = 5;
    state.cols_eliminated = 3;
    state.bounds_propagated = 7;
    state.flip_count = 2;
    state.phase = 2;
    state.num_vars = 10;
    state.num_constrs = 5;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);

    TEST_ASSERT_EQUAL_INT(42, snap[0]);
    TEST_ASSERT_EQUAL_INT(5, snap[3]);
    TEST_ASSERT_EQUAL_INT(3, snap[4]);
    TEST_ASSERT_EQUAL_INT(7, snap[5]);
    TEST_ASSERT_EQUAL_INT(2, snap[6]);
    TEST_ASSERT_EQUAL_INT(2, snap[7]);
}

void test_basis_diff_no_progress(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 10;
    state.num_vars = 10;
    state.num_constrs = 5;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);
    double diff = cxf_basis_diff(&state, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, diff);
}

void test_basis_diff_with_progress(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.num_vars = 10;
    state.num_constrs = 5;
    state.iteration = 10;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);

    state.iteration = 20;
    state.cols_eliminated = 3;

    double diff = cxf_basis_diff(&state, snap);
    /* V2 6-term formula. d_iter=10, d_cols=3, d_nvars=0. nnzDenom=1.
     * colDenom=10, rowDenom = (5-0)+0+0 = 5. */
    double t2 = 1.0 * 3.0 / 10.0;   /* column reduction (net_col=3) */
    double t3 = 0.25 * 10.0 / 10.0;  /* iteration (d_iter only) */
    double t6 = 0.1 * 10.0 / 5.0;    /* work counter (d_iter) */
    double expected = t2 + t3 + t6;
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expected, diff);
}

void test_basis_diff_six_terms(void) {
    /* Validate all 6 terms of the V2 weighted score formula per
     * basis_operations.md §cxf_basis_diff. */
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.num_vars = 20;
    state.num_constrs = 10;
    state.num_nonzeros = 80;
    state.iteration = 0;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);

    state.iteration = 10;
    state.cols_eliminated = 2;
    state.rows_eliminated = 1;
    state.bounds_propagated = 4;
    state.ftran_count = 8;
    state.flip_count = 3;
    state.perturb_count = 2;
    state.degenerate_count = 5;
    state.ineq_to_eq_count = 3;
    state.matrix_transitions = 1;

    double diff = cxf_basis_diff(&state, snap);
    double t1 = 4.0  * 2.0 / 80.0;
    double t2 = 1.0  * 2.0 / 20.0;   /* d_nvars=0, net_col=2 */
    double t3 = 0.25 * (double)(0+10+3+5) / 20.0;
    double t4 = 1.0  * (double)(1+1+4+8) / 10.0;  /* rows+mtrans+props+ftran */
    double t5 = 0.5  * 3.0 / 10.0;   /* ineq_to_eq_count */
    double t6 = 0.1  * 10.0 / 10.0;
    double expected = t1 + t2 + t3 + t4 + t5 + t6;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, diff);
}

void test_basis_diff_null_returns_zero(void) {
    int snap[CXF_SNAPSHOT_SIZE] = {0};
    double diff = cxf_basis_diff(NULL, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, diff);
}

void test_basis_diff_null_snapshot_returns_zero(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.num_vars = 10;
    state.num_constrs = 5;
    double diff = cxf_basis_diff(&state, NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, diff);
}

void test_basis_snapshot_null_safe(void) {
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(NULL, snap);  /* Should not crash */
}

void test_basis_snapshot_null_buf_safe(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    cxf_progress_snapshot(&state, NULL);  /* Should not crash */
}

void test_basis_snapshot_preserves_on_update(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 5;
    state.num_vars = 4;
    state.num_constrs = 2;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);
    TEST_ASSERT_EQUAL_INT(5, snap[0]);

    state.iteration = 15;
    TEST_ASSERT_EQUAL_INT(5, snap[0]);
}

/*--- BasisSnapshot equal/free tests ---*/

void test_snapshot_equal_true(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_full(basis, &snap1, 0);
    cxf_basis_snapshot_full(basis, &snap2, 0);

    int equal = cxf_basis_snapshot_full_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, equal);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_equal_false(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1;
    cxf_basis_snapshot_full(basis, &snap1, 0);

    basis->basic_vars[1] = 2;
    BasisSnapshot snap2;
    cxf_basis_snapshot_full(basis, &snap2, 0);

    int equal = cxf_basis_snapshot_full_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(0, equal);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_free_null_safe(void) {
    cxf_basis_snapshot_full_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_snapshot_free_clears_valid(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    cxf_basis_snapshot_full(basis, &snap, 0);

    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    cxf_basis_snapshot_full_free(&snap);
    TEST_ASSERT_EQUAL_INT(0, snap.valid);

    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basis_snapshot_captures_counters);
    RUN_TEST(test_basis_diff_no_progress);
    RUN_TEST(test_basis_diff_with_progress);
    RUN_TEST(test_basis_diff_six_terms);
    RUN_TEST(test_basis_diff_null_returns_zero);
    RUN_TEST(test_basis_diff_null_snapshot_returns_zero);
    RUN_TEST(test_basis_snapshot_null_safe);
    RUN_TEST(test_basis_snapshot_null_buf_safe);
    RUN_TEST(test_basis_snapshot_preserves_on_update);
    RUN_TEST(test_snapshot_equal_true);
    RUN_TEST(test_snapshot_equal_false);
    RUN_TEST(test_snapshot_free_null_safe);
    RUN_TEST(test_snapshot_free_clears_valid);
    return UNITY_END();
}
