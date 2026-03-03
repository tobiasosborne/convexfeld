/**
 * @file test_basis_validate.c
 * @brief Tests for basis validation and extended validation
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

/* Validation */
int cxf_basis_validate(BasisState *basis);

/* Validation flags for extended validation */
#define CXF_CHECK_COUNT       0x01
#define CXF_CHECK_BOUNDS      0x02
#define CXF_CHECK_DUPLICATES  0x04
#define CXF_CHECK_CONSISTENCY 0x10
#define CXF_CHECK_ALL         0xFF

/* Extended validation with flags */
int cxf_basis_validate_ex(BasisState *basis, int flags);

void setUp(void) {}
void tearDown(void) {}

void test_basis_validate_valid_basis(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 4;

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_duplicate_vars(void) {
    BasisState *basis = cxf_basis_create(3, 5);
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
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 10;  /* Out of bounds (n=5) */

    int status = cxf_basis_validate(basis);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_validate_negative_index(void) {
    BasisState *basis = cxf_basis_create(3, 5);
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

    int status = cxf_basis_validate_ex(basis, CXF_CHECK_COUNT);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_ex_check_all(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 2;
    basis->basic_vars[2] = 4;
    basis->var_status[0] = 0;
    basis->var_status[2] = 1;
    basis->var_status[4] = 2;

    int status = cxf_basis_validate_ex(basis, CXF_CHECK_ALL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

void test_basis_validate_ex_no_flags(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 0;  /* Duplicate, but no check */
    basis->basic_vars[2] = 10; /* Out of bounds, but no check */

    int status = cxf_basis_validate_ex(basis, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basis_validate_valid_basis);
    RUN_TEST(test_basis_validate_duplicate_vars);
    RUN_TEST(test_basis_validate_null_arg);
    RUN_TEST(test_basis_validate_empty_basis);
    RUN_TEST(test_basis_validate_out_of_bounds);
    RUN_TEST(test_basis_validate_negative_index);
    RUN_TEST(test_basis_validate_ex_check_count);
    RUN_TEST(test_basis_validate_ex_check_all);
    RUN_TEST(test_basis_validate_ex_no_flags);
    return UNITY_END();
}
