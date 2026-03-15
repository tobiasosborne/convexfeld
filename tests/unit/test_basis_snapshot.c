/**
 * @file test_basis_snapshot.c
 * @brief Tests for BasisSnapshot create and diff operations
 *        (split from test_basis.c).
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

/* Full BasisSnapshot API (heavy, implementation extension) */
int cxf_basis_snapshot_full(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);
int cxf_basis_snapshot_full_diff(const BasisSnapshot *s1, const BasisSnapshot *s2);
void cxf_basis_snapshot_full_free(BasisSnapshot *snapshot);

void setUp(void) {}
void tearDown(void) {}

/*--- BasisSnapshot create tests ---*/

void test_snapshot_create_copies_data(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 2;
    basis->basic_vars[1] = 4;
    basis->basic_vars[2] = 0;
    basis->var_status[0] = 2;                /* Basic in row 2 */
    basis->var_status[1] = CXF_VAR_AT_LOWER; /* Nonbasic */
    basis->var_status[2] = 0;                /* Basic in row 0 */
    basis->var_status[4] = 1;                /* Basic in row 1 */
    basis->iteration = 42;

    BasisSnapshot snap;
    int status = cxf_basis_snapshot_full(basis, &snap, 0);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    TEST_ASSERT_EQUAL_INT(5, snap.numVars);
    TEST_ASSERT_EQUAL_INT(3, snap.numConstrs);
    TEST_ASSERT_EQUAL_INT(42, snap.iteration);
    TEST_ASSERT_NOT_NULL(snap.basisHeader);
    TEST_ASSERT_NOT_NULL(snap.varStatus);

    TEST_ASSERT_EQUAL_INT(2, snap.basisHeader[0]);
    TEST_ASSERT_EQUAL_INT(4, snap.basisHeader[1]);
    TEST_ASSERT_EQUAL_INT(0, snap.basisHeader[2]);

    /* Modifying basis should not affect snapshot */
    basis->basic_vars[0] = 99;
    TEST_ASSERT_EQUAL_INT(2, snap.basisHeader[0]);

    cxf_basis_snapshot_full_free(&snap);
    cxf_basis_free(basis);
}

void test_snapshot_create_null_args(void) {
    BasisSnapshot snap;
    int status = cxf_basis_snapshot_full(NULL, &snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    BasisState *basis = cxf_basis_create(2, 3);
    status = cxf_basis_snapshot_full(basis, NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_snapshot_create_empty_basis(void) {
    BasisState *basis = cxf_basis_create(0, 0);
    BasisSnapshot snap;

    int status = cxf_basis_snapshot_full(basis, &snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    TEST_ASSERT_EQUAL_INT(0, snap.numConstrs);
    TEST_ASSERT_EQUAL_INT(0, snap.numVars);

    cxf_basis_snapshot_full_free(&snap);
    cxf_basis_free(basis);
}

/*--- BasisSnapshot diff tests ---*/

void test_snapshot_diff_identical(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 3;
    basis->var_status[1] = 0;
    basis->var_status[2] = 1;
    basis->var_status[3] = 2;

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_full(basis, &snap1, 0);
    cxf_basis_snapshot_full(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_full_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(0, diff);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_one_header_change(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 3;

    BasisSnapshot snap1;
    cxf_basis_snapshot_full(basis, &snap1, 0);

    basis->basic_vars[1] = 4;
    BasisSnapshot snap2;
    cxf_basis_snapshot_full(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_full_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, diff);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_var_status_change(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    basis->var_status[0] = 0;
    basis->var_status[1] = 1;
    basis->var_status[2] = CXF_VAR_AT_LOWER;

    BasisSnapshot snap1;
    cxf_basis_snapshot_full(basis, &snap1, 0);

    basis->var_status[2] = CXF_VAR_AT_UPPER;
    BasisSnapshot snap2;
    cxf_basis_snapshot_full(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_full_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, diff);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_dimension_mismatch(void) {
    BasisState *basis1 = cxf_basis_create(2, 3);
    BasisState *basis2 = cxf_basis_create(3, 4);

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_full(basis1, &snap1, 0);
    cxf_basis_snapshot_full(basis2, &snap2, 0);

    int diff = cxf_basis_snapshot_full_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(-1, diff);

    cxf_basis_snapshot_full_free(&snap1);
    cxf_basis_snapshot_full_free(&snap2);
    cxf_basis_free(basis1);
    cxf_basis_free(basis2);
}

void test_snapshot_diff_null_args(void) {
    BasisSnapshot snap;
    snap.valid = 1;
    snap.numVars = 1;
    snap.numConstrs = 1;

    int diff = cxf_basis_snapshot_full_diff(NULL, &snap);
    TEST_ASSERT_EQUAL_INT(-1, diff);

    diff = cxf_basis_snapshot_full_diff(&snap, NULL);
    TEST_ASSERT_EQUAL_INT(-1, diff);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_create_copies_data);
    RUN_TEST(test_snapshot_create_null_args);
    RUN_TEST(test_snapshot_create_empty_basis);
    RUN_TEST(test_snapshot_diff_identical);
    RUN_TEST(test_snapshot_diff_one_header_change);
    RUN_TEST(test_snapshot_diff_var_status_change);
    RUN_TEST(test_snapshot_diff_dimension_mismatch);
    RUN_TEST(test_snapshot_diff_null_args);
    return UNITY_END();
}
