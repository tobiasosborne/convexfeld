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

/* Basis snapshot/comparison - to be implemented in M5.1.7 */
int *cxf_basis_snapshot(BasisState *basis);
int cxf_basis_diff(const int *snap1, const int *snap2, int m);
int cxf_basis_equal(BasisState *basis, const int *snapshot, int m);

/* Validation/warm start - to be implemented in M5.1.8 */
int cxf_basis_validate(BasisState *basis);
int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m);

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
 * Validation/warm start tests
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

    /* Snapshot/comparison tests */
    RUN_TEST(test_basis_snapshot_returns_copy);
    RUN_TEST(test_basis_diff_identical);
    RUN_TEST(test_basis_diff_one_change);
    RUN_TEST(test_basis_diff_all_different);
    RUN_TEST(test_basis_equal_true);
    RUN_TEST(test_basis_equal_false);

    /* Validation/warm start tests */
    RUN_TEST(test_basis_validate_valid_basis);
    RUN_TEST(test_basis_validate_duplicate_vars);
    RUN_TEST(test_basis_warm_loads_basis);
    RUN_TEST(test_basis_warm_clears_eta_list);

    return UNITY_END();
}
