/**
 * @file test_basis_lifecycle.c
 * @brief Tests for BasisState/EtaVector creation, destruction, initialization,
 *        and refactorization (split from test_basis.c).
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
int cxf_basis_init(BasisState *basis, int m, int n);

/* EtaVector lifecycle */
EtaVector *cxf_eta_create(int type, int pivot_row, int nnz);
void cxf_eta_free(EtaVector *eta);

/* Refactorization */
int cxf_fix_variables_at_bounds(BasisState *basis);

void setUp(void) {}
void tearDown(void) {}

/*--- BasisState creation/free tests ---*/

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

/*--- EtaVector creation/free tests ---*/

void test_eta_create_type1(void) {
    EtaVector *eta = cxf_eta_create(1, 2, 5);  /* Type 1, pivot row 2, 5 nnz */
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
    EtaVector *eta = cxf_eta_create(2, 0, 3);  /* Type 2, pivot row 0, 3 nnz */
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(2, eta->type);
    cxf_eta_free(eta);
}

void test_eta_free_null_safe(void) {
    cxf_eta_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_eta_create_empty(void) {
    EtaVector *eta = cxf_eta_create(1, 0, 0);  /* Empty eta */
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(0, eta->nnz);
    cxf_eta_free(eta);
}

/*--- cxf_fix_variables_at_bounds tests ---*/

void test_basis_refactor_clears_eta_list(void) {
    BasisState *basis = cxf_basis_create(3, 3);
    basis->eta_count = 5;
    basis->pivots_since_refactor = 10;
    int status = cxf_fix_variables_at_bounds(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);
    TEST_ASSERT_EQUAL_INT(0, basis->pivots_since_refactor);
    TEST_ASSERT_NULL(basis->eta_head);
    cxf_basis_free(basis);
}

void test_basis_refactor_identity_basis(void) {
    BasisState *basis = cxf_basis_create(2, 2);
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    int status = cxf_fix_variables_at_bounds(basis);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    cxf_basis_free(basis);
}

void test_basis_refactor_null_arg(void) {
    int status = cxf_fix_variables_at_bounds(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basis_create_returns_valid_struct);
    RUN_TEST(test_basis_free_null_safe);
    RUN_TEST(test_basis_create_zero_constraints);
    RUN_TEST(test_basis_init_sets_arrays);
    RUN_TEST(test_eta_create_type1);
    RUN_TEST(test_eta_create_type2);
    RUN_TEST(test_eta_free_null_safe);
    RUN_TEST(test_eta_create_empty);
    RUN_TEST(test_basis_refactor_clears_eta_list);
    RUN_TEST(test_basis_refactor_identity_basis);
    RUN_TEST(test_basis_refactor_null_arg);
    return UNITY_END();
}
