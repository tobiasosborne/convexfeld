/**
 * @file test_basis_diff_scoring.c
 * @brief Tests for V2 basis_diff scoring formula (convexfeld-3xyi).
 *
 * Validates the 6-term weighted formula per basis_operations.md:
 *   T1: structural (perturb delta) / total_nnz, heavy weight (4.0)
 *   T2: column reduction (d_cols - d_nvars) / colDenom, unit weight
 *   T3: 4 iteration counters / colDenom, light weight (0.25)
 *   T4: row stats (rows + mtrans + props + ftran) / rowDenom, unit
 *   T5: conversion (ineq_to_eq) / rowDenom, moderate weight (0.5)
 *   T6: work (iteration) / rowDenom, own weight (0.1)
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <string.h>
#include <math.h>

/* Declarations — V2 signatures with external snapshot buffer */
void cxf_progress_snapshot(SolverState *state, int *snapshot);
double cxf_basis_diff(SolverState *state, const int *snapshot);

void setUp(void) {}
void tearDown(void) {}

/* Helper: create zeroed state with given dimensions */
static SolverState make_state(int n, int m, int64_t nnz) {
    SolverState s;
    memset(&s, 0, sizeof(s));
    s.num_vars = n;
    s.num_constrs = m;
    s.num_nonzeros = nnz;
    return s;
}

/* Helper: compute rowDenom = (m - snap_rows) + snap_rows + snap_props */
static double row_denom(int m, int snap_rows, int snap_props) {
    int v = (m - snap_rows) + snap_rows + snap_props;
    return (double)(v > 1 ? v : 1);
}

/* Helper: compute colDenom = n - snap_cols, floor 1 */
static double col_denom(int n, int snap_cols) {
    int v = n - snap_cols;
    return (double)(v > 1 ? v : 1);
}

void test_term1_structural_uses_nnz(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.perturb_count = 5;
    double score = cxf_basis_diff(&s, snap);
    double expected = 4.0 * 5.0 / 100.0;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term2_column_reduction(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.cols_eliminated = 3;
    double score = cxf_basis_diff(&s, snap);
    double cd = col_denom(20, 0);
    /* d_nvars=0 so net_col = d_cols = 3 */
    double expected = 1.0 * 3.0 / cd;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term3_iteration_counters_four(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.iteration = 10;
    s.flip_count = 4;
    s.degenerate_count = 2;
    double cd = col_denom(20, 0);
    double rd = row_denom(10, 0, 0);
    double t3 = 0.25 * (double)(0 + 10 + 4 + 2) / cd;
    double t6 = 0.1 * 10.0 / rd;
    double expected = t3 + t6;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term4_row_statistics(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.rows_eliminated = 3;
    s.bounds_propagated = 5;
    /* T4 = (d_rows + d_mtrans + d_props + d_ftran) / rowDenom
     * d_mtrans=0, d_ftran=0 so sum = 3+0+5+0 = 8 */
    double rd = row_denom(10, 0, 0);
    double expected = 1.0 * (3.0 + 5.0) / rd;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term5_conversion(void) {
    /* T5 now uses ineq_to_eq_count, not ftran_count */
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.ineq_to_eq_count = 6;
    double rd = row_denom(10, 0, 0);
    double expected = 0.5 * 6.0 / rd;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term6_work_counter(void) {
    SolverState s = make_state(20, 10, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.iteration = 8;
    double cd = col_denom(20, 0);
    double rd = row_denom(10, 0, 0);
    double t3 = 0.25 * 8.0 / cd;
    double t6 = 0.1 * 8.0 / rd;
    double expected = t3 + t6;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_all_six_terms_combined(void) {
    SolverState s = make_state(40, 20, 200);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);

    s.perturb_count = 3;
    s.cols_eliminated = 2;
    s.iteration = 12;
    s.flip_count = 4;
    s.degenerate_count = 1;
    s.rows_eliminated = 3;
    s.bounds_propagated = 7;
    s.ftran_count = 9;
    s.ineq_to_eq_count = 5;
    s.matrix_transitions = 2;

    double nnzD = 200.0;
    double cd = col_denom(40, 0);
    double rd = row_denom(20, 0, 0);

    double t1 = 4.0  * 3.0 / nnzD;
    double t2 = 1.0  * 2.0 / cd;   /* d_nvars=0 */
    double t3 = 0.25 * (double)(0 + 12 + 4 + 1) / cd;
    double t4 = 1.0  * (double)(3 + 2 + 7 + 9) / rd;
    double t5 = 0.5  * 5.0 / rd;
    double t6 = 0.1  * 12.0 / rd;
    double expected = t1 + t2 + t3 + t4 + t5 + t6;

    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_rowdenom_includes_snapshot_props(void) {
    SolverState s = make_state(20, 20, 100);
    int snap[CXF_SNAPSHOT_SIZE];
    s.bounds_propagated = 10;
    cxf_progress_snapshot(&s, snap);

    s.rows_eliminated = 2;
    s.bounds_propagated = 15;
    double rd = row_denom(20, 0, 10);
    /* T4 = (d_rows + d_mtrans + d_props + d_ftran) / rowDenom
     * = (2 + 0 + 5 + 0) / rd */
    double expected = 1.0 * (2.0 + 5.0) / rd;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_nnz_floor_prevents_div_zero(void) {
    SolverState s = make_state(10, 5, 0);
    int snap[CXF_SNAPSHOT_SIZE];
    cxf_progress_snapshot(&s, snap);
    s.perturb_count = 2;
    double score = cxf_basis_diff(&s, snap);
    double expected = 4.0 * 2.0 / 1.0;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_negative_deltas_clamped(void) {
    SolverState s = make_state(10, 5, 50);
    int snap[CXF_SNAPSHOT_SIZE];
    s.iteration = 100;
    s.perturb_count = 10;
    cxf_progress_snapshot(&s, snap);
    s.iteration = 50;
    s.perturb_count = 5;
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, score);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_term1_structural_uses_nnz);
    RUN_TEST(test_term2_column_reduction);
    RUN_TEST(test_term3_iteration_counters_four);
    RUN_TEST(test_term4_row_statistics);
    RUN_TEST(test_term5_conversion);
    RUN_TEST(test_term6_work_counter);
    RUN_TEST(test_all_six_terms_combined);
    RUN_TEST(test_rowdenom_includes_snapshot_props);
    RUN_TEST(test_nnz_floor_prevents_div_zero);
    RUN_TEST(test_negative_deltas_clamped);
    return UNITY_END();
}
