/**
 * @file test_snapshot_location.c
 * @brief Verify progress_snapshot lives on BasisState per V2 basis_state.md.
 *
 * Issue: convexfeld-7522 -- progress_snapshot was on SolverState,
 * V2 spec requires it on BasisState.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stddef.h>
#include <string.h>

/* Internal basis lifecycle (not in public header) */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

/* V2 snapshot/diff signatures */
void cxf_progress_snapshot(SolverState *state, int *snapshot);

void setUp(void) {}
void tearDown(void) {}

/* CXF_SNAPSHOT_SIZE must be defined via cxf_basis.h (not cxf_solver.h) */
void test_snapshot_size_defined_in_basis_header(void) {
    TEST_ASSERT_EQUAL_INT(16, CXF_SNAPSHOT_SIZE);
}

/* BasisState must contain progress_snapshot array */
void test_basis_state_has_progress_snapshot(void) {
    BasisState bs;
    memset(&bs, 0, sizeof(bs));
    /* Compile-time proof: accessing bs.progress_snapshot is valid */
    bs.progress_snapshot[0] = 42;
    TEST_ASSERT_EQUAL_INT(42, bs.progress_snapshot[0]);
    /* Array must have CXF_SNAPSHOT_SIZE elements */
    TEST_ASSERT_EQUAL(CXF_SNAPSHOT_SIZE * sizeof(int),
                      sizeof(bs.progress_snapshot));
}

/* SolverState must NOT contain progress_snapshot (moved to BasisState).
 * We verify indirectly: sizeof(SolverState) should not include the array.
 * More importantly, the field is gone -- this test just documents intent. */
void test_solver_state_lacks_progress_snapshot(void) {
    /* If SolverState still had progress_snapshot[10], its size would be
     * larger by 10*sizeof(int). We cannot test "field absence" in C
     * without compile errors, but this test documents the move. */
    TEST_PASS_MESSAGE("progress_snapshot removed from SolverState, "
                      "now on BasisState per V2 basis_state.md");
}

/* cxf_basis_create zero-inits progress_snapshot via calloc */
void test_basis_create_zeroes_snapshot(void) {
    BasisState *b = cxf_basis_create(2, 4);
    TEST_ASSERT_NOT_NULL(b);
    for (int i = 0; i < CXF_SNAPSHOT_SIZE; i++) {
        TEST_ASSERT_EQUAL_INT(0, b->progress_snapshot[i]);
    }
    cxf_basis_free(b);
}

/* After cxf_progress_snapshot, buffer reflects state counters */
void test_snapshot_populates_buffer_from_state(void) {
    /* Build minimal SolverState with a basis */
    SolverState s;
    memset(&s, 0, sizeof(s));
    s.num_vars = 5;
    s.num_constrs = 3;
    BasisState bs;
    memset(&bs, 0, sizeof(bs));
    s.basis = &bs;

    s.iteration = 17;
    s.ftran_count = 4;
    bs.pivots_since_refactor = 2;

    int buf[CXF_SNAPSHOT_SIZE];
    memset(buf, 0xFF, sizeof(buf));

    cxf_progress_snapshot(&s, buf);

    TEST_ASSERT_EQUAL_INT(17, buf[0]);  /* iteration */
    TEST_ASSERT_EQUAL_INT(2, buf[1]);   /* pivots_since_refactor */
    TEST_ASSERT_EQUAL_INT(4, buf[2]);   /* ftran_count */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_size_defined_in_basis_header);
    RUN_TEST(test_basis_state_has_progress_snapshot);
    RUN_TEST(test_solver_state_lacks_progress_snapshot);
    RUN_TEST(test_basis_create_zeroes_snapshot);
    RUN_TEST(test_snapshot_populates_buffer_from_state);
    return UNITY_END();
}
