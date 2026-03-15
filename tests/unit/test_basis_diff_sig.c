/**
 * @file test_basis_diff_sig.c
 * @brief Tests for V2 cxf_basis_diff / cxf_progress_snapshot signatures.
 *
 * Validates convexfeld-67bt: both functions take an explicit snapshot
 * buffer per basis_operations.md, allowing comparison against arbitrary
 * snapshots rather than a single internal buffer.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

/* V2 signatures — external snapshot buffer */
void cxf_progress_snapshot(SolverState *state, int *snapshot);
double cxf_basis_diff(SolverState *state, const int *snapshot);

void setUp(void) {}
void tearDown(void) {}

static SolverState make_state(int n, int m) {
    SolverState s;
    memset(&s, 0, sizeof(s));
    s.num_vars = n;
    s.num_constrs = m;
    s.num_nonzeros = 100;
    return s;
}

/* Core contract: snapshot writes to caller buffer, diff reads from it */
void test_snapshot_writes_to_external_buffer(void) {
    SolverState s = make_state(10, 5);
    s.iteration = 42;
    s.ftran_count = 7;
    int buf[CXF_SNAPSHOT_SIZE];
    memset(buf, 0, sizeof(buf));

    cxf_progress_snapshot(&s, buf);

    TEST_ASSERT_EQUAL_INT(42, buf[0]);
    TEST_ASSERT_EQUAL_INT(7, buf[2]);
}

/* Two independent snapshots can coexist */
void test_two_independent_snapshots(void) {
    SolverState s = make_state(20, 10);
    int snap_a[CXF_SNAPSHOT_SIZE];
    int snap_b[CXF_SNAPSHOT_SIZE];

    s.iteration = 10;
    cxf_progress_snapshot(&s, snap_a);

    s.iteration = 50;
    cxf_progress_snapshot(&s, snap_b);

    /* snap_a still holds old value */
    TEST_ASSERT_EQUAL_INT(10, snap_a[0]);
    TEST_ASSERT_EQUAL_INT(50, snap_b[0]);
}

/* diff uses the passed-in snapshot, not any internal state */
void test_diff_uses_passed_snapshot(void) {
    SolverState s = make_state(20, 10);
    int snap_a[CXF_SNAPSHOT_SIZE];
    int snap_b[CXF_SNAPSHOT_SIZE];

    s.iteration = 10;
    cxf_progress_snapshot(&s, snap_a);

    s.iteration = 50;
    cxf_progress_snapshot(&s, snap_b);

    s.iteration = 60;

    /* diff against snap_a: d_iter=50 */
    double diff_a = cxf_basis_diff(&s, snap_a);
    /* diff against snap_b: d_iter=10 */
    double diff_b = cxf_basis_diff(&s, snap_b);

    /* snap_a has larger delta so must produce larger score */
    TEST_ASSERT_TRUE(diff_a > diff_b);
    TEST_ASSERT_TRUE(diff_b > 0.0);
}

/* Diff against a zero-filled snapshot (simulates fresh start) */
void test_diff_against_zeroed_snapshot(void) {
    SolverState s = make_state(10, 5);
    s.iteration = 20;
    s.cols_eliminated = 3;
    int snap[CXF_SNAPSHOT_SIZE];
    memset(snap, 0, sizeof(snap));

    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_TRUE(score > 0.0);
}

/* Null state returns 0.0 */
void test_null_state(void) {
    int snap[CXF_SNAPSHOT_SIZE] = {0};
    double score = cxf_basis_diff(NULL, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, score);
}

/* Null snapshot returns 0.0 */
void test_null_snapshot(void) {
    SolverState s = make_state(10, 5);
    double score = cxf_basis_diff(&s, NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, score);
}

/* Null state for snapshot is a no-op */
void test_snapshot_null_state_noop(void) {
    int buf[CXF_SNAPSHOT_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    cxf_progress_snapshot(NULL, buf);
    /* Buffer should be unchanged (function returned early) */
    TEST_ASSERT_EQUAL_INT(-1, buf[0]);  /* 0xFF...FF = -1 as signed int */
}

/* Null buffer for snapshot is a no-op */
void test_snapshot_null_buf_noop(void) {
    SolverState s = make_state(10, 5);
    s.iteration = 99;
    cxf_progress_snapshot(&s, NULL);  /* must not crash */
    TEST_PASS();
}

/* Same snapshot can be reused after overwriting */
void test_snapshot_overwrite(void) {
    SolverState s = make_state(10, 5);
    int snap[CXF_SNAPSHOT_SIZE];

    s.iteration = 10;
    cxf_progress_snapshot(&s, snap);
    TEST_ASSERT_EQUAL_INT(10, snap[0]);

    s.iteration = 30;
    cxf_progress_snapshot(&s, snap);
    TEST_ASSERT_EQUAL_INT(30, snap[0]);

    /* No further progress -> diff = 0 */
    double score = cxf_basis_diff(&s, snap);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, score);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_writes_to_external_buffer);
    RUN_TEST(test_two_independent_snapshots);
    RUN_TEST(test_diff_uses_passed_snapshot);
    RUN_TEST(test_diff_against_zeroed_snapshot);
    RUN_TEST(test_null_state);
    RUN_TEST(test_null_snapshot);
    RUN_TEST(test_snapshot_null_state_noop);
    RUN_TEST(test_snapshot_null_buf_noop);
    RUN_TEST(test_snapshot_overwrite);
    return UNITY_END();
}
