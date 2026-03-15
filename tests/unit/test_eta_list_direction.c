/**
 * @file test_eta_list_direction.c
 * @brief Tests that eta linked list direction matches V2 spec.
 *
 * V2 spec (eta_vector.md):
 *   - "next" field links to the next OLDER eta vector
 *   - New etas are prepended at head (newest at head)
 *   - List ordered: head (newest) -> ... -> tail (oldest)
 *   - FTRAN applies oldest to newest
 *   - BTRAN applies newest to oldest
 *
 * Beads: convexfeld-pxh0
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/* Forward declarations */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_pivot_with_eta(BasisState *basis, int pivotRow, const double *pivotCol,
                       int enteringVar, int leavingVar, int leavingStatus,
                       double reducedCost, int direction);
int cxf_ftran(BasisState *basis, const double *column, double *result);
int cxf_btran(BasisState *basis, int row, double *result);

static BasisState *basis = NULL;

void setUp(void) {
    basis = cxf_basis_create(3, 6);
    TEST_ASSERT_NOT_NULL(basis);
    basis->basic_vars[0] = 3;
    basis->basic_vars[1] = 4;
    basis->basic_vars[2] = 5;
    for (int j = 0; j < 3; j++)
        basis->var_status[j] = CXF_VAR_AT_LOWER;
    basis->var_status[3] = 0;
    basis->var_status[4] = 1;
    basis->var_status[5] = 2;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
}

/*---------------------------------------------------------------------------
 * Test: after one pivot, head->next is NULL (single element)
 *---------------------------------------------------------------------------*/
void test_single_eta_next_is_null(void) {
    double col[] = {2.0, 0.5, 0.1};
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1));
    TEST_ASSERT_NOT_NULL(basis->eta_head);
    TEST_ASSERT_NULL(basis->eta_head->next);
}

/*---------------------------------------------------------------------------
 * Test: after two pivots, head is the NEWER eta, head->next is OLDER
 *
 * Per spec: "next" links to the next older eta vector.
 * Pivot 1 creates eta with pivot_row=0 (older).
 * Pivot 2 creates eta with pivot_row=1 (newer, becomes head).
 * So head->pivot_row == 1 (newer) and head->next->pivot_row == 0 (older).
 *---------------------------------------------------------------------------*/
void test_two_pivots_head_is_newer_next_is_older(void) {
    double col1[] = {2.0, 0.5, 0.1};
    double col2[] = {0.1, 1.5, 0.3};

    /* Pivot 1: entering var 0 replaces var 3 in row 0 */
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_pivot_with_eta(basis, 0, col1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1));

    /* Pivot 2: entering var 1 replaces var 4 in row 1 */
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_pivot_with_eta(basis, 1, col2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1));

    TEST_ASSERT_EQUAL_INT(2, basis->eta_count);

    /* Head is newer (pivot 2, row 1) */
    EtaVector *head = basis->eta_head;
    TEST_ASSERT_NOT_NULL(head);
    TEST_ASSERT_EQUAL_INT(1, head->pivot_row);

    /* head->next is older (pivot 1, row 0) */
    EtaVector *older = head->next;
    TEST_ASSERT_NOT_NULL(older);
    TEST_ASSERT_EQUAL_INT(0, older->pivot_row);

    /* Oldest has no next */
    TEST_ASSERT_NULL(older->next);
}

/*---------------------------------------------------------------------------
 * Test: three pivots preserve newest-to-oldest ordering
 *---------------------------------------------------------------------------*/
void test_three_pivots_newest_to_oldest(void) {
    double col1[] = {2.0, 0.5, 0.1};
    double col2[] = {0.1, 1.5, 0.3};
    double col3[] = {0.2, 0.3, 1.8};

    cxf_pivot_with_eta(basis, 0, col1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    cxf_pivot_with_eta(basis, 1, col2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);
    cxf_pivot_with_eta(basis, 2, col3, 2, 5, CXF_VAR_AT_LOWER, 0.0, 1);

    TEST_ASSERT_EQUAL_INT(3, basis->eta_count);

    /* Traverse: head (newest, row 2) -> middle (row 1) -> tail (oldest, row 0) */
    EtaVector *e = basis->eta_head;
    TEST_ASSERT_EQUAL_INT(2, e->pivot_row);   /* newest */
    e = e->next;
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT(1, e->pivot_row);   /* middle */
    e = e->next;
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT(0, e->pivot_row);   /* oldest */
    TEST_ASSERT_NULL(e->next);                /* terminates */
}

/*---------------------------------------------------------------------------
 * Test: list length matches eta_count
 *
 * Spec invariant 2: "the chain contains exactly as many nodes as the
 * SolverState's eta total count, and terminates with a null pointer."
 *---------------------------------------------------------------------------*/
void test_list_length_matches_eta_count(void) {
    double col1[] = {2.0, 0.5, 0.1};
    double col2[] = {0.1, 1.5, 0.3};
    double col3[] = {0.2, 0.3, 1.8};

    cxf_pivot_with_eta(basis, 0, col1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    cxf_pivot_with_eta(basis, 1, col2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);
    cxf_pivot_with_eta(basis, 2, col3, 2, 5, CXF_VAR_AT_LOWER, 0.0, 1);

    int count = 0;
    for (EtaVector *e = basis->eta_head; e != NULL; e = e->next)
        count++;
    TEST_ASSERT_EQUAL_INT(basis->eta_count, count);
}

/*---------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_eta_next_is_null);
    RUN_TEST(test_two_pivots_head_is_newer_next_is_older);
    RUN_TEST(test_three_pivots_newest_to_oldest);
    RUN_TEST(test_list_length_matches_eta_count);
    return UNITY_END();
}
