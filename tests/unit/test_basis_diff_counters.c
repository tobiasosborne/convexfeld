/**
 * @file test_basis_diff_counters.c
 * @brief Tests for V2 spec-required snapshot counters (convexfeld-gicm).
 *
 * Validates cxf_progress_snapshot captures all basis_operations.md fields
 * and cxf_basis_diff uses them in the 6-term formula correctly.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <string.h>
#include <math.h>

void cxf_progress_snapshot(SolverState *state, int *snapshot);
double cxf_basis_diff(SolverState *state, const int *snapshot);

void setUp(void) {}
void tearDown(void) {}

static SolverState make_state(int n, int m, int64_t nnz) {
    SolverState s;
    memset(&s, 0, sizeof(s));
    s.num_vars = n;
    s.num_constrs = m;
    s.num_nonzeros = nnz;
    return s;
}

/* Snapshot captures problem dimensions */
void test_snapshot_captures_dimensions(void) {
    SolverState s = make_state(42, 37, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    TEST_ASSERT_EQUAL_INT(42, snap[10]);
    TEST_ASSERT_EQUAL_INT(37, snap[11]);
}

/* Snapshot captures status flags */
void test_snapshot_captures_status_flags(void) {
    SolverState s = make_state(10, 5, 50);
    s.sol_status = 7;
    s.perturb_expand_active = 1;
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    TEST_ASSERT_EQUAL_INT(7, snap[12]);
    TEST_ASSERT_EQUAL_INT(1, snap[13]);
}

/* Snapshot captures new iteration counters */
void test_snapshot_captures_new_counters(void) {
    SolverState s = make_state(10, 5, 50);
    s.ineq_to_eq_count = 9;
    s.matrix_transitions = 4;
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    TEST_ASSERT_EQUAL_INT(9, snap[14]);
    TEST_ASSERT_EQUAL_INT(4, snap[15]);
}

void test_snapshot_size_is_16(void) {
    TEST_ASSERT_EQUAL_INT(16, CXF_SNAPSHOT_SIZE);
}

/* Term 2: net removed cols subtracts numVars increase */
void test_term2_subtracts_numvars_increase(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.cols_eliminated = 5;
    s.num_vars = 23;  /* d_nvars=3, net_col = 5-3 = 2 */
    /* colDenom uses snapshot numVars (20) */
    double cd = 20.0;
    double expected = 1.0 * 2.0 / cd;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

/* Term 2: negative net clamped to 0 */
void test_term2_negative_net_clamped(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.cols_eliminated = 2;
    s.num_vars = 25;  /* d_nvars=5, net_col = 2-5 = -3 -> 0 */
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, score);
}

/* Term 4: includes matrix_transitions */
void test_term4_includes_matrix_transitions(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.matrix_transitions = 3;
    double expected = 1.0 * 3.0 / 10.0;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

/* Term 4: includes ftran_count (per-constraint activity proxy) */
void test_term4_includes_ftran(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.ftran_count = 7;
    double expected = 1.0 * 7.0 / 10.0;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

/* Term 5: uses ineq_to_eq_count */
void test_term5_uses_ineq_to_eq(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.ineq_to_eq_count = 4;
    double expected = 0.5 * 4.0 / 10.0;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

/* All new counters combine correctly in full 6-term formula */
void test_all_new_counters_combined(void) {
    SolverState s = make_state(30, 15, 150);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);

    s.perturb_count = 2;
    s.cols_eliminated = 4;
    s.num_vars = 31;          /* d_nvars = 1 */
    s.iteration = 8;
    s.flip_count = 3;
    s.degenerate_count = 1;
    s.rows_eliminated = 2;
    s.matrix_transitions = 3;
    s.bounds_propagated = 4;
    s.ftran_count = 6;
    s.ineq_to_eq_count = 5;

    double nnzD = 150.0;
    double cd = 30.0;
    double rd = 15.0;

    int net_col = 4 - 1;
    double t1 = 4.0  * 2.0 / nnzD;
    double t2 = 1.0  * (double)net_col / cd;
    double t3 = 0.25 * (double)(0 + 8 + 3 + 1) / cd;
    double t4 = 1.0  * (double)(2 + 3 + 4 + 6) / rd;
    double t5 = 0.5  * 5.0 / rd;
    double t6 = 0.1  * 8.0 / rd;
    double expected = t1 + t2 + t3 + t4 + t5 + t6;

    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_captures_dimensions);
    RUN_TEST(test_snapshot_captures_status_flags);
    RUN_TEST(test_snapshot_captures_new_counters);
    RUN_TEST(test_snapshot_size_is_16);
    RUN_TEST(test_term2_subtracts_numvars_increase);
    RUN_TEST(test_term2_negative_net_clamped);
    RUN_TEST(test_term4_includes_matrix_transitions);
    RUN_TEST(test_term4_includes_ftran);
    RUN_TEST(test_term5_uses_ineq_to_eq);
    RUN_TEST(test_all_new_counters_combined);
    return UNITY_END();
}
