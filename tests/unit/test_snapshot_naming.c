/**
 * @file test_snapshot_naming.c
 * @brief Tests verifying the two snapshot mechanisms are distinct.
 *
 * Two snapshot mechanisms exist:
 *   1. Lightweight (V2 spec): cxf_progress_snapshot() + cxf_basis_diff()
 *      in basis_stub.c -- scalar counters, O(1), returns double score.
 *   2. Heavy (extension): cxf_basis_snapshot_full() + cxf_basis_snapshot_full_diff()
 *      in snapshot.c -- copies basisHeader + varStatus, O(m+n), returns int count.
 *
 * These tests confirm:
 *   - Both mechanisms compile and link under their correct names
 *   - The lightweight snapshot captures counters (not arrays)
 *   - The heavy snapshot captures arrays (not just counters)
 *   - The diff return types differ: double (lightweight) vs int (heavy)
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

/* Lightweight V2 spec snapshot (basis_stub.c) */
void cxf_progress_snapshot(SolverState *state, int *snapshot);
double cxf_basis_diff(SolverState *state, const int *snapshot);

/* Heavy full BasisSnapshot (snapshot.c) */
int cxf_basis_snapshot_full(BasisState *basis, BasisSnapshot *snapshot,
                            int includeFactors);
int cxf_basis_snapshot_full_diff(const BasisSnapshot *s1,
                                 const BasisSnapshot *s2);
int cxf_basis_snapshot_full_equal(const BasisSnapshot *s1,
                                  const BasisSnapshot *s2);
void cxf_basis_snapshot_full_free(BasisSnapshot *snapshot);

void setUp(void) {}
void tearDown(void) {}

/*--- Lightweight (V2 spec) snapshot tests ---*/

void test_lightweight_snapshot_captures_counters(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 77;
    state.num_vars = 10;
    state.num_constrs = 5;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);
    TEST_ASSERT_EQUAL_INT(77, snap[0]);
}

void test_lightweight_diff_returns_double(void) {
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.num_vars = 10;
    state.num_constrs = 5;
    state.iteration = 0;

    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, snap);

    state.iteration = 10;
    double score = cxf_basis_diff(&state, snap);
    TEST_ASSERT_TRUE(score > 0.0);
}

/*--- Heavy (extension) snapshot tests ---*/

void test_heavy_snapshot_copies_arrays(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    basis->basic_vars[0] = 5;
    basis->basic_vars[1] = 7;
    basis->var_status[0] = CXF_VAR_AT_LOWER;
    basis->var_status[1] = 1;
    basis->var_status[2] = CXF_VAR_AT_UPPER;

    BasisSnapshot snap;
    int rc = cxf_basis_snapshot_full(basis, &snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    TEST_ASSERT_EQUAL_INT(5, snap.basisHeader[0]);
    TEST_ASSERT_EQUAL_INT(7, snap.basisHeader[1]);
    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_UPPER, snap.varStatus[2]);

    cxf_basis_snapshot_full_free(&snap);
    cxf_basis_free(basis);
}

void test_heavy_diff_returns_int_count(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;

    BasisSnapshot s1, s2;
    cxf_basis_snapshot_full(basis, &s1, 0);

    basis->basic_vars[0] = 2;
    cxf_basis_snapshot_full(basis, &s2, 0);

    int count = cxf_basis_snapshot_full_diff(&s1, &s2);
    TEST_ASSERT_EQUAL_INT(1, count);

    cxf_basis_snapshot_full_free(&s1);
    cxf_basis_snapshot_full_free(&s2);
    cxf_basis_free(basis);
}

void test_heavy_equal_uses_full_diff(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;

    BasisSnapshot s1, s2;
    cxf_basis_snapshot_full(basis, &s1, 0);
    cxf_basis_snapshot_full(basis, &s2, 0);

    TEST_ASSERT_EQUAL_INT(1, cxf_basis_snapshot_full_equal(&s1, &s2));

    cxf_basis_snapshot_full_free(&s1);
    cxf_basis_snapshot_full_free(&s2);
    cxf_basis_free(basis);
}

void test_heavy_free_clears_valid(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    cxf_basis_snapshot_full(basis, &snap, 0);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);

    cxf_basis_snapshot_full_free(&snap);
    TEST_ASSERT_EQUAL_INT(0, snap.valid);
    TEST_ASSERT_NULL(snap.basisHeader);
    TEST_ASSERT_NULL(snap.varStatus);

    cxf_basis_free(basis);
}

/*--- Cross-mechanism: both coexist without collision ---*/

void test_both_mechanisms_coexist(void) {
    /* Lightweight snapshot uses SolverState + int buffer */
    SolverState state;
    memset(&state, 0, sizeof(state));
    state.iteration = 42;
    state.num_vars = 4;
    state.num_constrs = 2;

    int counter_snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&state, counter_snap);
    TEST_ASSERT_EQUAL_INT(42, counter_snap[0]);

    /* Heavy snapshot uses BasisState + BasisSnapshot struct */
    BasisState *basis = cxf_basis_create(2, 4);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;

    BasisSnapshot full_snap;
    int rc = cxf_basis_snapshot_full(basis, &full_snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, full_snap.basisHeader[0]);

    cxf_basis_snapshot_full_free(&full_snap);
    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    /* Lightweight (V2 spec) */
    RUN_TEST(test_lightweight_snapshot_captures_counters);
    RUN_TEST(test_lightweight_diff_returns_double);
    /* Heavy (extension) */
    RUN_TEST(test_heavy_snapshot_copies_arrays);
    RUN_TEST(test_heavy_diff_returns_int_count);
    RUN_TEST(test_heavy_equal_uses_full_diff);
    RUN_TEST(test_heavy_free_clears_valid);
    /* Coexistence */
    RUN_TEST(test_both_mechanisms_coexist);
    return UNITY_END();
}
