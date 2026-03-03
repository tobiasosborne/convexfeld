/**
 * @file test_basis_btran.c
 * @brief Tests for BTRAN and BTRAN_VEC (backward transformation) operations
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

/* BTRAN */
int cxf_btran(BasisState *basis, int row, double *result);
int cxf_btran_vec(BasisState *basis, const double *input, double *result);

void setUp(void) {}
void tearDown(void) {}

/*--- cxf_btran tests (solve y^T B = e_i^T) ---*/

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

/*--- cxf_btran_vec tests (BTRAN with arbitrary input vector) ---*/

void test_btran_vec_identity_basis(void) {
    /* Identity basis: B = I, so B^(-T) = I, result = input */
    BasisState *basis = cxf_basis_create(3, 3);

    double input[] = {1.0, 2.0, 3.0};
    double result[3] = {0.0};

    int status = cxf_btran_vec(basis, input, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, result[2]);

    cxf_basis_free(basis);
}

void test_btran_vec_zero_input(void) {
    BasisState *basis = cxf_basis_create(2, 2);

    double input[] = {0.0, 0.0};
    double result[2] = {99.0, 99.0};  /* Pre-fill to verify zeros written */

    int status = cxf_btran_vec(basis, input, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);

    cxf_basis_free(basis);
}

void test_btran_vec_single_constraint(void) {
    BasisState *basis = cxf_basis_create(1, 1);

    double input[] = {5.0};
    double result[1] = {0.0};

    int status = cxf_btran_vec(basis, input, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    /* For identity basis, result = input */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, result[0]);

    cxf_basis_free(basis);
}

void test_btran_vec_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 2);
    double input[] = {1.0, 2.0};
    double result[2];

    int status = cxf_btran_vec(NULL, input, result);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_btran_vec(basis, NULL, result);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_btran_vec(basis, input, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_btran_vec_empty_basis(void) {
    BasisState *basis = cxf_basis_create(0, 0);

    /* For empty basis (m=0), use dummy non-NULL arrays */
    double input_dummy = 0.0;
    double result_dummy = 0.0;

    /* Should handle empty basis gracefully - m=0 means early return */
    int status = cxf_btran_vec(basis, &input_dummy, &result_dummy);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_btran_identity_basis);
    RUN_TEST(test_btran_last_row);
    RUN_TEST(test_btran_single_constraint);
    RUN_TEST(test_btran_invalid_row);
    RUN_TEST(test_btran_vec_identity_basis);
    RUN_TEST(test_btran_vec_zero_input);
    RUN_TEST(test_btran_vec_single_constraint);
    RUN_TEST(test_btran_vec_null_args);
    RUN_TEST(test_btran_vec_empty_basis);
    return UNITY_END();
}
