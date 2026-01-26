/**
 * @file test_basis.c
 * @brief TDD tests for basis operations (M5.1.1)
 *
 * Tests for BasisState, EtaFactors, FTRAN, BTRAN, refactorization.
 * ~250 LOC of comprehensive tests.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/*******************************************************************************
 * External function declarations (to be implemented)
 ******************************************************************************/

/* BasisState lifecycle - to be implemented in M5.1.2 */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_basis_init(BasisState *basis, int m, int n);

/* EtaFactors lifecycle - to be implemented in M5.1.3 */
EtaFactors *cxf_eta_create(int type, int pivot_row, int nnz);
void cxf_eta_free(EtaFactors *eta);

/* FTRAN/BTRAN - to be implemented in M5.1.4 / M5.1.5 */
int cxf_ftran(BasisState *basis, const double *column, double *result);
int cxf_btran(BasisState *basis, int row, double *result);

/* Refactorization - to be implemented in M5.1.6 */
int cxf_basis_refactor(BasisState *basis);

/* Basis snapshot/comparison - legacy API (still in basis_stub.c) */
int *cxf_basis_snapshot(BasisState *basis);
int cxf_basis_diff(const int *snap1, const int *snap2, int m);
int cxf_basis_equal(BasisState *basis, const int *snapshot, int m);

/* BasisSnapshot API - implemented in snapshot.c (M5.1.7) */
int cxf_basis_snapshot_create(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);
int cxf_basis_snapshot_diff(const BasisSnapshot *s1, const BasisSnapshot *s2);
int cxf_basis_snapshot_equal(const BasisSnapshot *s1, const BasisSnapshot *s2);
void cxf_basis_snapshot_free(BasisSnapshot *snapshot);

/* Validation/warm start - implemented in warm.c (M5.1.8) */
int cxf_basis_validate(BasisState *basis);
int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m);

/* Validation flags for extended validation */
#define CXF_CHECK_COUNT       0x01
#define CXF_CHECK_BOUNDS      0x02
#define CXF_CHECK_DUPLICATES  0x04
#define CXF_CHECK_CONSISTENCY 0x10
#define CXF_CHECK_ALL         0xFF

/* Extended validation with flags */
int cxf_basis_validate_ex(BasisState *basis, int flags);

/* Warm start from BasisSnapshot */
int cxf_basis_warm_snapshot(BasisState *basis, const BasisSnapshot *snapshot);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * BasisState creation/free tests
 ******************************************************************************/

void test_basis_create_returns_valid_struct(void) {
    BasisState *basis = cxf_basis_create(3, 5);  /* 3 constraints, 5 variables */
    TEST_ASSERT_NOT_NULL(basis);
    TEST_ASSERT_EQUAL_INT(3, basis->m);
    TEST_ASSERT_NOT_NULL(basis->basic_vars);
    TEST_ASSERT_NOT_NULL(basis->var_status);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);
    cxf_basis_free(basis);
}

void test_basis_free_null_safe(void) {
    cxf_basis_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_basis_create_zero_constraints(void) {
    BasisState *basis = cxf_basis_create(0, 0);
    TEST_ASSERT_NOT_NULL(basis);
    TEST_ASSERT_EQUAL_INT(0, basis->m);
    cxf_basis_free(basis);
}

void test_basis_init_sets_arrays(void) {
    BasisState *basis = cxf_basis_create(4, 6);
    TEST_ASSERT_NOT_NULL(basis);
    TEST_ASSERT_NOT_NULL(basis->work);
    TEST_ASSERT_EQUAL_INT(0, basis->pivots_since_refactor);
    cxf_basis_free(basis);
}

/*******************************************************************************
 * EtaFactors creation/free tests
 ******************************************************************************/

void test_eta_create_type1(void) {
    EtaFactors *eta = cxf_eta_create(1, 2, 5);  /* Type 1, pivot row 2, 5 nnz */
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(1, eta->type);
    TEST_ASSERT_EQUAL_INT(2, eta->pivot_row);
    TEST_ASSERT_EQUAL_INT(5, eta->nnz);
    TEST_ASSERT_NOT_NULL(eta->indices);
    TEST_ASSERT_NOT_NULL(eta->values);
    TEST_ASSERT_NULL(eta->next);
    cxf_eta_free(eta);
}

void test_eta_create_type2(void) {
    EtaFactors *eta = cxf_eta_create(2, 0, 3);  /* Type 2, pivot row 0, 3 nnz */
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(2, eta->type);
    cxf_eta_free(eta);
}

void test_eta_free_null_safe(void) {
    cxf_eta_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_eta_create_empty(void) {
    EtaFactors *eta = cxf_eta_create(1, 0, 0);  /* Empty eta */
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(0, eta->nnz);
    cxf_eta_free(eta);
}

/*******************************************************************************
 * cxf_ftran tests (forward transformation: solve Bx = b)
 ******************************************************************************/

void test_ftran_identity_basis(void) {
    /* Identity basis: B = I, so x = b */
    BasisState *basis = cxf_basis_create(3, 3);

    /* Setup identity basis (basic vars are slacks) */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    basis->basic_vars[2] = 2;

    double column[] = {1.0, 2.0, 3.0};
    double result[3] = {0.0};

    int status = cxf_ftran(basis, column, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, result[2]);

    cxf_basis_free(basis);
}

void test_ftran_zero_column(void) {
    BasisState *basis = cxf_basis_create(2, 2);

    double column[] = {0.0, 0.0};
    double result[2] = {99.0, 99.0};  /* Pre-fill to verify zeros written */

    int status = cxf_ftran(basis, column, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);

    cxf_basis_free(basis);
}

void test_ftran_unit_vector(void) {
    /* FTRAN of e_i should give column i of B^(-1) */
    BasisState *basis = cxf_basis_create(3, 3);

    double column[] = {1.0, 0.0, 0.0};  /* e_0 */
    double result[3] = {0.0};

    int status = cxf_ftran(basis, column, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    /* For identity basis, result should equal column */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);

    cxf_basis_free(basis);
}

void test_ftran_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 2);
    double result[2];

    int status = cxf_ftran(NULL, NULL, result);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

/*******************************************************************************
 * cxf_btran tests (backward transformation: solve y^T B = e_i^T)
 ******************************************************************************/

void test_btran_identity_basis(void) {
    /* Identity basis: y = e_row */
    BasisState *basis = cxf_basis_create(3, 3);

    double result[3] = {0.0};

    int status = cxf_btran(basis, 0, result);  /* Row 0 */
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[2]);

    cxf_basis_free(basis);
}

void test_btran_last_row(void) {
    BasisState *basis = cxf_basis_create(3, 3);

    double result[3] = {0.0};

    int status = cxf_btran(basis, 2, result);  /* Last row */
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[2]);

    cxf_basis_free(basis);
}

void test_btran_single_constraint(void) {
    BasisState *basis = cxf_basis_create(1, 1);

    double result[1] = {0.0};

    int status = cxf_btran(basis, 0, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    /* For single identity, result[0] = 1/B[0,0] = 1/1 = 1 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);

    cxf_basis_free(basis);
}

void test_btran_invalid_row(void) {
    BasisState *basis = cxf_basis_create(3, 3);
    double result[3];

    int status = cxf_btran(basis, -1, result);  /* Invalid row */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    status = cxf_btran(basis, 5, result);  /* Row out of bounds */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

/*******************************************************************************
 * cxf_basis_refactor tests
 ******************************************************************************/

void test_basis_refactor_clears_eta_list(void) {
    BasisState *basis = cxf_basis_create(3, 3);

    /* Simulate having some etas */
    basis->eta_count = 5;
    basis->pivots_since_refactor = 10;

    int status = cxf_basis_refactor(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);
    TEST_ASSERT_EQUAL_INT(0, basis->pivots_since_refactor);
    TEST_ASSERT_NULL(basis->eta_head);

    cxf_basis_free(basis);
}

void test_basis_refactor_identity_basis(void) {
    BasisState *basis = cxf_basis_create(2, 2);

    /* Set up identity basis */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;

    int status = cxf_basis_refactor(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_refactor_null_arg(void) {
    int status = cxf_basis_refactor(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

/*******************************************************************************
 * Basis snapshot/comparison tests
 ******************************************************************************/

void test_basis_snapshot_returns_copy(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    basis->basic_vars[0] = 2;
    basis->basic_vars[1] = 4;
    basis->basic_vars[2] = 0;

    int *snapshot = cxf_basis_snapshot(basis);
    TEST_ASSERT_NOT_NULL(snapshot);
    TEST_ASSERT_EQUAL_INT(2, snapshot[0]);
    TEST_ASSERT_EQUAL_INT(4, snapshot[1]);
    TEST_ASSERT_EQUAL_INT(0, snapshot[2]);

    /* Modifying basis shouldn't affect snapshot */
    basis->basic_vars[0] = 99;
    TEST_ASSERT_EQUAL_INT(2, snapshot[0]);

    free(snapshot);
    cxf_basis_free(basis);
}

void test_basis_diff_identical(void) {
    int snap1[] = {1, 2, 3};
    int snap2[] = {1, 2, 3};

    int diff = cxf_basis_diff(snap1, snap2, 3);
    TEST_ASSERT_EQUAL_INT(0, diff);  /* No differences */
}

void test_basis_diff_one_change(void) {
    int snap1[] = {1, 2, 3};
    int snap2[] = {1, 5, 3};  /* Position 1 differs */

    int diff = cxf_basis_diff(snap1, snap2, 3);
    TEST_ASSERT_EQUAL_INT(1, diff);  /* One difference */
}

void test_basis_diff_all_different(void) {
    int snap1[] = {1, 2, 3};
    int snap2[] = {4, 5, 6};

    int diff = cxf_basis_diff(snap1, snap2, 3);
    TEST_ASSERT_EQUAL_INT(3, diff);  /* All different */
}

void test_basis_equal_true(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    int snapshot[] = {1, 3, 4};

    int equal = cxf_basis_equal(basis, snapshot, 3);
    TEST_ASSERT_EQUAL_INT(1, equal);  /* Equal */

    cxf_basis_free(basis);
}

void test_basis_equal_false(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    int snapshot[] = {1, 2, 4};  /* Position 1 differs */

    int equal = cxf_basis_equal(basis, snapshot, 3);
    TEST_ASSERT_EQUAL_INT(0, equal);  /* Not equal */

    cxf_basis_free(basis);
}

/*******************************************************************************
 * BasisSnapshot API tests (M5.1.7)
 ******************************************************************************/

void test_snapshot_create_copies_data(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 2;
    basis->basic_vars[1] = 4;
    basis->basic_vars[2] = 0;
    basis->var_status[0] = CXF_BASIC;
    basis->var_status[1] = CXF_NONBASIC_L;
    basis->var_status[2] = CXF_BASIC;
    basis->iteration = 42;

    BasisSnapshot snap;
    int status = cxf_basis_snapshot_create(basis, &snap, 0);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    TEST_ASSERT_EQUAL_INT(5, snap.numVars);
    TEST_ASSERT_EQUAL_INT(3, snap.numConstrs);
    TEST_ASSERT_EQUAL_INT(42, snap.iteration);
    TEST_ASSERT_NOT_NULL(snap.basisHeader);
    TEST_ASSERT_NOT_NULL(snap.varStatus);

    /* Check basisHeader copied correctly */
    TEST_ASSERT_EQUAL_INT(2, snap.basisHeader[0]);
    TEST_ASSERT_EQUAL_INT(4, snap.basisHeader[1]);
    TEST_ASSERT_EQUAL_INT(0, snap.basisHeader[2]);

    /* Modifying basis should not affect snapshot */
    basis->basic_vars[0] = 99;
    TEST_ASSERT_EQUAL_INT(2, snap.basisHeader[0]);

    cxf_basis_snapshot_free(&snap);
    cxf_basis_free(basis);
}

void test_snapshot_create_null_args(void) {
    BasisSnapshot snap;
    int status = cxf_basis_snapshot_create(NULL, &snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    BasisState *basis = cxf_basis_create(2, 3);
    status = cxf_basis_snapshot_create(basis, NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_snapshot_create_empty_basis(void) {
    BasisState *basis = cxf_basis_create(0, 0);
    BasisSnapshot snap;

    int status = cxf_basis_snapshot_create(basis, &snap, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    TEST_ASSERT_EQUAL_INT(0, snap.numConstrs);
    TEST_ASSERT_EQUAL_INT(0, snap.numVars);

    cxf_basis_snapshot_free(&snap);
    cxf_basis_free(basis);
}

void test_snapshot_diff_identical(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 3;
    basis->var_status[0] = CXF_BASIC;
    basis->var_status[1] = CXF_BASIC;

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_create(basis, &snap1, 0);
    cxf_basis_snapshot_create(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(0, diff);

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_one_header_change(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 3;

    BasisSnapshot snap1;
    cxf_basis_snapshot_create(basis, &snap1, 0);

    basis->basic_vars[1] = 4;  /* Change one value */
    BasisSnapshot snap2;
    cxf_basis_snapshot_create(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, diff);

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_var_status_change(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    basis->var_status[0] = CXF_BASIC;
    basis->var_status[1] = CXF_BASIC;
    basis->var_status[2] = CXF_NONBASIC_L;

    BasisSnapshot snap1;
    cxf_basis_snapshot_create(basis, &snap1, 0);

    basis->var_status[2] = CXF_NONBASIC_U;  /* Change var status */
    BasisSnapshot snap2;
    cxf_basis_snapshot_create(basis, &snap2, 0);

    int diff = cxf_basis_snapshot_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, diff);  /* One varStatus differs */

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_diff_dimension_mismatch(void) {
    BasisState *basis1 = cxf_basis_create(2, 3);
    BasisState *basis2 = cxf_basis_create(3, 4);

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_create(basis1, &snap1, 0);
    cxf_basis_snapshot_create(basis2, &snap2, 0);

    int diff = cxf_basis_snapshot_diff(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(-1, diff);  /* Dimension mismatch */

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis1);
    cxf_basis_free(basis2);
}

void test_snapshot_diff_null_args(void) {
    BasisSnapshot snap;
    snap.valid = 1;
    snap.numVars = 1;
    snap.numConstrs = 1;

    int diff = cxf_basis_snapshot_diff(NULL, &snap);
    TEST_ASSERT_EQUAL_INT(-1, diff);

    diff = cxf_basis_snapshot_diff(&snap, NULL);
    TEST_ASSERT_EQUAL_INT(-1, diff);
}

void test_snapshot_equal_true(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1, snap2;
    cxf_basis_snapshot_create(basis, &snap1, 0);
    cxf_basis_snapshot_create(basis, &snap2, 0);

    int equal = cxf_basis_snapshot_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(1, equal);

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_equal_false(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 3;
    basis->basic_vars[2] = 4;

    BasisSnapshot snap1;
    cxf_basis_snapshot_create(basis, &snap1, 0);

    basis->basic_vars[1] = 2;  /* Change value */
    BasisSnapshot snap2;
    cxf_basis_snapshot_create(basis, &snap2, 0);

    int equal = cxf_basis_snapshot_equal(&snap1, &snap2);
    TEST_ASSERT_EQUAL_INT(0, equal);

    cxf_basis_snapshot_free(&snap1);
    cxf_basis_snapshot_free(&snap2);
    cxf_basis_free(basis);
}

void test_snapshot_free_null_safe(void) {
    cxf_basis_snapshot_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_snapshot_free_clears_valid(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    cxf_basis_snapshot_create(basis, &snap, 0);

    TEST_ASSERT_EQUAL_INT(1, snap.valid);
    cxf_basis_snapshot_free(&snap);
    TEST_ASSERT_EQUAL_INT(0, snap.valid);

    cxf_basis_free(basis);
}

/*******************************************************************************
 * Validation/warm start tests (M5.1.8)
 ******************************************************************************/

void test_basis_validate_valid_basis(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    /* Valid basis: 3 distinct variables in [0,5) */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 4;

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_duplicate_vars(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    /* Invalid: duplicate basic variable */
    basis->basic_vars[0] = 1;
    basis->basic_vars[1] = 1;  /* Duplicate! */
    basis->basic_vars[2] = 2;

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_validate_null_arg(void) {
    int status = cxf_basis_validate(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_basis_validate_empty_basis(void) {
    BasisState *basis = cxf_basis_create(0, 0);
    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);  /* Trivially valid */
    cxf_basis_free(basis);
}

void test_basis_validate_out_of_bounds(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    /* Invalid: variable index out of bounds */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 10;  /* Out of bounds (n=5) */

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_validate_negative_index(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    /* Invalid: negative variable index */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = -1;  /* Invalid */
    basis->basic_vars[2] = 2;

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_validate_ex_check_count(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    basis->basic_vars[2] = 2;

    /* With only count check, should pass */
    int status = cxf_basis_validate_ex(basis, CXF_CHECK_COUNT);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_ex_check_all(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 4;

    /* With all checks, valid basis should pass */
    int status = cxf_basis_validate_ex(basis, CXF_CHECK_ALL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_ex_no_flags(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 0;  /* Duplicate, but no check */
    basis->basic_vars[2] = 10; /* Out of bounds, but no check */

    /* With no flags, returns OK immediately */
    int status = cxf_basis_validate_ex(basis, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_warm_loads_basis(void) {
    BasisState *basis = cxf_basis_create(3, 5);

    int warm_vars[] = {1, 3, 4};

    int status = cxf_basis_warm(basis, warm_vars, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, basis->basic_vars[0]);
    TEST_ASSERT_EQUAL_INT(3, basis->basic_vars[1]);
    TEST_ASSERT_EQUAL_INT(4, basis->basic_vars[2]);

    cxf_basis_free(basis);
}

void test_basis_warm_clears_eta_list(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    basis->eta_count = 10;  /* Simulate existing etas */

    int warm_vars[] = {0, 2};

    int status = cxf_basis_warm(basis, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);  /* Etas cleared */

    cxf_basis_free(basis);
}

void test_basis_warm_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    int warm_vars[] = {0, 1};

    int status = cxf_basis_warm(NULL, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_basis_warm(basis, NULL, 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_size_mismatch(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    int warm_vars[] = {0, 1, 2};  /* 3 vars, but basis has m=2 */

    int status = cxf_basis_warm(basis, warm_vars, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_resets_pivot_count(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    basis->pivots_since_refactor = 50;  /* Simulate pivots */

    int warm_vars[] = {0, 2};

    int status = cxf_basis_warm(basis, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->pivots_since_refactor);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_copies_basis(void) {
    /* Create source basis and snapshot */
    BasisState *source = cxf_basis_create(3, 5);
    source->basic_vars[0] = 1;
    source->basic_vars[1] = 3;
    source->basic_vars[2] = 4;
    source->var_status[0] = CXF_BASIC;
    source->var_status[1] = CXF_NONBASIC_L;
    source->var_status[2] = CXF_BASIC;

    BasisSnapshot snap;
    cxf_basis_snapshot_create(source, &snap, 0);

    /* Create target basis and warm start from snapshot */
    BasisState *target = cxf_basis_create(3, 5);
    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Verify basis copied */
    TEST_ASSERT_EQUAL_INT(1, target->basic_vars[0]);
    TEST_ASSERT_EQUAL_INT(3, target->basic_vars[1]);
    TEST_ASSERT_EQUAL_INT(4, target->basic_vars[2]);

    /* Verify var status copied */
    TEST_ASSERT_EQUAL_INT(CXF_BASIC, target->var_status[0]);
    TEST_ASSERT_EQUAL_INT(CXF_NONBASIC_L, target->var_status[1]);
    TEST_ASSERT_EQUAL_INT(CXF_BASIC, target->var_status[2]);

    cxf_basis_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

void test_basis_warm_snapshot_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    snap.valid = 1;

    int status = cxf_basis_warm_snapshot(NULL, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_basis_warm_snapshot(basis, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_invalid_snap(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    snap.valid = 0;  /* Invalid snapshot */

    int status = cxf_basis_warm_snapshot(basis, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_dimension_mismatch(void) {
    BasisState *source = cxf_basis_create(3, 5);
    source->basic_vars[0] = 0;
    source->basic_vars[1] = 1;
    source->basic_vars[2] = 2;

    BasisSnapshot snap;
    cxf_basis_snapshot_create(source, &snap, 0);

    /* Create target with different dimensions */
    BasisState *target = cxf_basis_create(2, 4);
    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

void test_basis_warm_snapshot_clears_etas(void) {
    BasisState *source = cxf_basis_create(2, 3);
    source->basic_vars[0] = 0;
    source->basic_vars[1] = 1;

    BasisSnapshot snap;
    cxf_basis_snapshot_create(source, &snap, 0);

    BasisState *target = cxf_basis_create(2, 3);
    target->eta_count = 15;
    target->pivots_since_refactor = 25;

    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, target->eta_count);
    TEST_ASSERT_EQUAL_INT(0, target->pivots_since_refactor);

    cxf_basis_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* BasisState tests */
    RUN_TEST(test_basis_create_returns_valid_struct);
    RUN_TEST(test_basis_free_null_safe);
    RUN_TEST(test_basis_create_zero_constraints);
    RUN_TEST(test_basis_init_sets_arrays);

    /* EtaFactors tests */
    RUN_TEST(test_eta_create_type1);
    RUN_TEST(test_eta_create_type2);
    RUN_TEST(test_eta_free_null_safe);
    RUN_TEST(test_eta_create_empty);

    /* FTRAN tests */
    RUN_TEST(test_ftran_identity_basis);
    RUN_TEST(test_ftran_zero_column);
    RUN_TEST(test_ftran_unit_vector);
    RUN_TEST(test_ftran_null_args);

    /* BTRAN tests */
    RUN_TEST(test_btran_identity_basis);
    RUN_TEST(test_btran_last_row);
    RUN_TEST(test_btran_single_constraint);
    RUN_TEST(test_btran_invalid_row);

    /* Refactorization tests */
    RUN_TEST(test_basis_refactor_clears_eta_list);
    RUN_TEST(test_basis_refactor_identity_basis);
    RUN_TEST(test_basis_refactor_null_arg);

    /* Legacy snapshot/comparison tests */
    RUN_TEST(test_basis_snapshot_returns_copy);
    RUN_TEST(test_basis_diff_identical);
    RUN_TEST(test_basis_diff_one_change);
    RUN_TEST(test_basis_diff_all_different);
    RUN_TEST(test_basis_equal_true);
    RUN_TEST(test_basis_equal_false);

    /* BasisSnapshot API tests (M5.1.7) */
    RUN_TEST(test_snapshot_create_copies_data);
    RUN_TEST(test_snapshot_create_null_args);
    RUN_TEST(test_snapshot_create_empty_basis);
    RUN_TEST(test_snapshot_diff_identical);
    RUN_TEST(test_snapshot_diff_one_header_change);
    RUN_TEST(test_snapshot_diff_var_status_change);
    RUN_TEST(test_snapshot_diff_dimension_mismatch);
    RUN_TEST(test_snapshot_diff_null_args);
    RUN_TEST(test_snapshot_equal_true);
    RUN_TEST(test_snapshot_equal_false);
    RUN_TEST(test_snapshot_free_null_safe);
    RUN_TEST(test_snapshot_free_clears_valid);

    /* Validation tests (M5.1.8) */
    RUN_TEST(test_basis_validate_valid_basis);
    RUN_TEST(test_basis_validate_duplicate_vars);
    RUN_TEST(test_basis_validate_null_arg);
    RUN_TEST(test_basis_validate_empty_basis);
    RUN_TEST(test_basis_validate_out_of_bounds);
    RUN_TEST(test_basis_validate_negative_index);
    RUN_TEST(test_basis_validate_ex_check_count);
    RUN_TEST(test_basis_validate_ex_check_all);
    RUN_TEST(test_basis_validate_ex_no_flags);

    /* Warm start tests (M5.1.8) */
    RUN_TEST(test_basis_warm_loads_basis);
    RUN_TEST(test_basis_warm_clears_eta_list);
    RUN_TEST(test_basis_warm_null_args);
    RUN_TEST(test_basis_warm_size_mismatch);
    RUN_TEST(test_basis_warm_resets_pivot_count);
    RUN_TEST(test_basis_warm_snapshot_copies_basis);
    RUN_TEST(test_basis_warm_snapshot_null_args);
    RUN_TEST(test_basis_warm_snapshot_invalid_snap);
    RUN_TEST(test_basis_warm_snapshot_dimension_mismatch);
    RUN_TEST(test_basis_warm_snapshot_clears_etas);

    return UNITY_END();
}
