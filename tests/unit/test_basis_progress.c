/**
 * @file test_basis_progress.c
 * @brief Tests for legacy progress snapshot/diff and BasisSnapshot
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

/* Legacy progress snapshot/diff */
void cxf_progress_snapshot(SolverState *state);
double cxf_basis_diff(SolverState *state);

/* BasisSnapshot API */
int cxf_progress_snapshot_create(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);
int cxf_progress_snapshot_equal(const BasisSnapshot *s1, const BasisSnapshot *s2);
void cxf_progress_snapshot_free(BasisSnapshot *snapshot);

void setUp(void) {}
void tearDown(void) {}

/*--- Legacy snapshot/comparison tests ---*/

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

    cxf_progress_snapshot(&state);

    TEST_ASSERT_EQUAL_INT(42, state.progress_snapshot[0]);
    TEST_ASSERT_EQUAL_INT(5, state.progress_snapshot[3]);
    TEST_ASSERT_EQUAL_INT(3, state.progress_snapshot[4]);
    TEST_ASSERT_EQUAL_INT(7, state.progress_snapshot[5]);
    TEST_ASSERT_EQUAL_INT(2, state.progress_snapshot[6]);
    TEST_ASSERT_EQUAL_INT(2, state.progress_snapshot[7]);
}

void test_basis_diff_no_progress(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 10;
    state.num_vars = 10;
    state.num_constrs = 5;

    cxf_progress_snapshot(&state);
    double diff = cxf_basis_diff(&state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, diff);
}

void test_basis_diff_with_progress(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.num_vars = 10;
    state.num_constrs = 5;
    state.iteration = 10;

    cxf_progress_snapshot(&state);

    state.iteration = 20;
    state.cols_eliminated = 3;

    double diff = cxf_basis_diff(&state);
    double expected = 4.0 * 3.0 / 10.0 + 0.25 * 10.0 / 10.0;
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expected, diff);
}

void test_basis_diff_null_returns_zero(void) {
    double diff = cxf_basis_diff(NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, diff);
}

void test_basis_snapshot_null_safe(void) {
    cxf_progress_snapshot(NULL);  /* Should not crash */
}

void test_basis_snapshot_preserves_on_update(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 5;
    state.num_vars = 4;
    state.num_constrs = 2;

    cxf_progress_snapshot(&state);
    TEST_ASSERT_EQUAL_INT(5, state.progress_snapshot[0]);

    state.iteration = 15;
    TEST_ASSERT_EQUAL_INT(5, state.progress_snapshot[0]);
}

/*--- BasisSnapshot equal/free tests ---*/

void test_snapshot_equal_true(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1, snap2;
    cxf_progress_snapshot_create(basis, &snap1, 0);
    cxf_progress_snapshot_create(basis, &snap2, 0);

    int equal = cxf_progress_snapshot_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, equal);

    cxf_progress_snapshot_free(&snap1);
    cxf_progress_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_equal_false(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1;
    cxf_progress_snapshot_create(basis, &snap1, 0);

    basis->basic_vars[1] = 2;
    BasisSnapshot snap2;
    cxf_progress_snapshot_create(basis, &snap2, 0);

    int equal = cxf_progress_snapshot_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(0, equal);

    cxf_progress_snapshot_free(&snap1);
    cxf_progress_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_free_null_safe(void) {
    cxf_progress_snapshot_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_snapshot_free_clears_valid(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    cxf_progress_snapshot_create(basis, &snap, 0);

    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    cxf_progress_snapshot_free(&snap);
    TEST_ASSERT_EQUAL_INT(0, snap.valid);

    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basis_snapshot_captures_counters);
    RUN_TEST(test_basis_diff_no_progress);
    RUN_TEST(test_basis_diff_with_progress);
    RUN_TEST(test_basis_diff_null_returns_zero);
    RUN_TEST(test_basis_snapshot_null_safe);
    RUN_TEST(test_basis_snapshot_preserves_on_update);
    RUN_TEST(test_snapshot_equal_true);
    RUN_TEST(test_snapshot_equal_false);
    RUN_TEST(test_snapshot_free_null_safe);
    RUN_TEST(test_snapshot_free_clears_valid);
    return UNITY_END();
}
