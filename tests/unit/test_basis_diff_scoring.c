/**
 * @file test_basis_diff_scoring.c
 * @brief Tests for V2 basis_diff scoring formula (convexfeld-3xyi).
 *
 * Validates the 6-term weighted formula per basis_operations.md:
 *   T1: structural (perturb delta) / total_nnz, heavy weight
 *   T2: column reduction / colDenom, unit weight
 *   T3: 4 iteration counters / colDenom, light weight
 *   T4: row stats (rows + props) / rowDenom, unit weight
 *   T5: conversion (ftran) / rowDenom, moderate weight
 *   T6: work (iteration) / rowDenom, own weight
 */

#include "unity.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <string.h>
#include <math.h>

/* Declarations */
void cxf_progress_snapshot(SolverState *state);
double cxf_basis_diff(SolverState *state);

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
    /* Term 1: 4.0 * d_perturb / nnzDenom */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.perturb_count = 5;
    double score = cxf_basis_diff(&s);
    double expected = 4.0 * 5.0 / 100.0;
    /* Other terms may contribute via d_perturb=0 for other terms, but
     * only term 1 uses perturb_count. Isolate by setting nothing else. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term2_column_reduction(void) {
    /* Term 2: 1.0 * d_cols / colDenom */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.cols_eliminated = 3;
    double score = cxf_basis_diff(&s);
    double cd = col_denom(20, 0);
    double expected = 1.0 * 3.0 / cd;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term3_iteration_counters_four(void) {
    /* Term 3: 0.25 * (d_piv + d_iter + d_flips + d_degen) / colDenom */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.iteration = 10;
    s.flip_count = 4;
    s.degenerate_count = 2;
    /* pivots_since_refactor needs basis; leave NULL => d_piv clamps to 0.
     * Term 6 also uses d_iter, so expected includes both. */
    double cd = col_denom(20, 0);
    double rd = row_denom(10, 0, 0);
    double t3 = 0.25 * (double)(0 + 10 + 4 + 2) / cd;
    double t6 = 0.1 * 10.0 / rd;
    double expected = t3 + t6;
    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term4_row_statistics(void) {
    /* Term 4: 1.0 * (d_rows + d_props) / rowDenom */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.rows_eliminated = 3;
    s.bounds_propagated = 5;
    double rd = row_denom(10, 0, 0);
    double expected = 1.0 * (3.0 + 5.0) / rd;
    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term5_conversion(void) {
    /* Term 5: 0.5 * d_ftran / rowDenom */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.ftran_count = 6;
    double rd = row_denom(10, 0, 0);
    double expected = 0.5 * 6.0 / rd;
    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_term6_work_counter(void) {
    /* Term 6: 0.1 * d_iter / rowDenom. d_iter also in term 3. */
    SolverState s = make_state(20, 10, 100);
    cxf_progress_snapshot(&s);
    s.iteration = 8;
    double cd = col_denom(20, 0);
    double rd = row_denom(10, 0, 0);
    double t3 = 0.25 * 8.0 / cd;  /* d_iter appears in term 3 too */
    double t6 = 0.1 * 8.0 / rd;
    double expected = t3 + t6;
    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_all_six_terms_combined(void) {
    /* All 6 terms active. Verify exact composite score.
     * basis=NULL so d_piv clamps to 0. Tests all other counters. */
    SolverState s = make_state(40, 20, 200);
    cxf_progress_snapshot(&s);

    s.perturb_count = 3;       /* T1 */
    s.cols_eliminated = 2;     /* T2 */
    s.iteration = 12;          /* T3 iter + T6 */
    s.flip_count = 4;          /* T3 flips */
    s.degenerate_count = 1;    /* T3 degen */
    s.rows_eliminated = 3;     /* T4 rows */
    s.bounds_propagated = 7;   /* T4 props */
    s.ftran_count = 9;         /* T5 */

    double nnzD = 200.0;
    double cd = col_denom(40, 0);     /* 40 */
    double rd = row_denom(20, 0, 0);  /* 20 */

    double t1 = 4.0  * 3.0 / nnzD;
    double t2 = 1.0  * 2.0 / cd;
    double t3 = 0.25 * (double)(0 + 12 + 4 + 1) / cd;  /* piv=0 */
    double t4 = 1.0  * (double)(3 + 7) / rd;
    double t5 = 0.5  * 9.0 / rd;
    double t6 = 0.1  * 12.0 / rd;
    double expected = t1 + t2 + t3 + t4 + t5 + t6;

    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_rowdenom_includes_snapshot_props(void) {
    /* rowDenom = (m - snap_rows) + snap_rows + snap_props.
     * With snap_props = 10, rowDenom = 20 + 10 = 30 (not 20). */
    SolverState s = make_state(20, 20, 100);
    s.bounds_propagated = 10;
    cxf_progress_snapshot(&s);

    s.rows_eliminated = 2;
    s.bounds_propagated = 15;  /* d_props = 5 */
    double rd = row_denom(20, 0, 10);  /* 20 + 0 + 10 = 30 */
    double expected = 1.0 * (2.0 + 5.0) / rd;
    double score = cxf_basis_diff(&s);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_nnz_floor_prevents_div_zero(void) {
    /* With num_nonzeros = 0, nnzDenom should floor to 1. */
    SolverState s = make_state(10, 5, 0);
    cxf_progress_snapshot(&s);
    s.perturb_count = 2;
    double score = cxf_basis_diff(&s);
    double expected = 4.0 * 2.0 / 1.0;  /* nnzDenom = 1 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, expected, score);
}

void test_negative_deltas_clamped(void) {
    /* Counter resets produce zero signal, not negative. */
    SolverState s = make_state(10, 5, 50);
    s.iteration = 100;
    s.perturb_count = 10;
    cxf_progress_snapshot(&s);
    s.iteration = 50;       /* went down */
    s.perturb_count = 5;    /* went down */
    double score = cxf_basis_diff(&s);
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
