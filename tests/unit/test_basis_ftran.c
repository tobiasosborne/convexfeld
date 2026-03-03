/**
 * @file test_basis_ftran.c
 * @brief Tests for FTRAN (forward transformation) operations
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

/* FTRAN */
int cxf_ftran(BasisState *basis, const double *column, double *result);

void setUp(void) {}
void tearDown(void) {}

void test_ftran_identity_basis(void) {
    /* Identity basis: B = I, so x = b */
    BasisState *basis = cxf_basis_create(3, 3);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ftran_identity_basis);
    RUN_TEST(test_ftran_zero_column);
    RUN_TEST(test_ftran_unit_vector);
    RUN_TEST(test_ftran_null_args);
    return UNITY_END();
}
