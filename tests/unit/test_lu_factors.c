/**
 * @file test_lu_factors.c
 * @brief Tests for LUFactors lifecycle functions.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>

/* External function declarations from basis_state.c */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * cxf_lu_create tests
 ******************************************************************************/

void test_lu_create_returns_valid_struct(void) {
    LUFactors *lu = cxf_lu_create(5, 10, 10);
    TEST_ASSERT_NOT_NULL(lu);
    TEST_ASSERT_EQUAL_INT(5, lu->m);
    TEST_ASSERT_EQUAL_INT(0, lu->valid);
    TEST_ASSERT_EQUAL_INT64(0, lu->L_nnz);
    TEST_ASSERT_EQUAL_INT64(0, lu->U_nnz);
    TEST_ASSERT_NOT_NULL(lu->L_col_ptr);
    TEST_ASSERT_NOT_NULL(lu->L_row_idx);
    TEST_ASSERT_NOT_NULL(lu->L_values);
    TEST_ASSERT_NOT_NULL(lu->U_col_ptr);
    TEST_ASSERT_NOT_NULL(lu->U_row_idx);
    TEST_ASSERT_NOT_NULL(lu->U_values);
    TEST_ASSERT_NOT_NULL(lu->U_diag);
    TEST_ASSERT_NOT_NULL(lu->perm_row);
    TEST_ASSERT_NOT_NULL(lu->perm_col);
    cxf_lu_free(lu);
}

void test_lu_create_initializes_permutations_to_identity(void) {
    LUFactors *lu = cxf_lu_create(4, 8, 8);
    TEST_ASSERT_NOT_NULL(lu);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(i, lu->perm_row[i]);
        TEST_ASSERT_EQUAL_INT(i, lu->perm_col[i]);
    }
    cxf_lu_free(lu);
}

void test_lu_create_zero_m_returns_null(void) {
    LUFactors *lu = cxf_lu_create(0, 0, 0);
    TEST_ASSERT_NULL(lu);
}

void test_lu_create_negative_m_returns_null(void) {
    LUFactors *lu = cxf_lu_create(-5, 10, 10);
    TEST_ASSERT_NULL(lu);
}

void test_lu_create_small_nnz_estimates_adjusted(void) {
    /* Small estimates should be adjusted to at least m */
    LUFactors *lu = cxf_lu_create(10, 1, 1);
    TEST_ASSERT_NOT_NULL(lu);
    /* Storage should be usable for at least m entries */
    cxf_lu_free(lu);
}

void test_lu_create_large_dimensions(void) {
    LUFactors *lu = cxf_lu_create(1000, 5000, 5000);
    TEST_ASSERT_NOT_NULL(lu);
    TEST_ASSERT_EQUAL_INT(1000, lu->m);
    cxf_lu_free(lu);
}

/*******************************************************************************
 * cxf_lu_free tests
 ******************************************************************************/

void test_lu_free_null_safe(void) {
    cxf_lu_free(NULL);  /* Should not crash */
    TEST_PASS();
}

/*******************************************************************************
 * cxf_lu_clear tests
 ******************************************************************************/

void test_lu_clear_resets_state(void) {
    LUFactors *lu = cxf_lu_create(3, 6, 6);
    TEST_ASSERT_NOT_NULL(lu);

    /* Simulate some factorization state */
    lu->valid = 1;
    lu->L_nnz = 5;
    lu->U_nnz = 7;
    lu->perm_row[0] = 2;
    lu->perm_row[1] = 0;
    lu->perm_row[2] = 1;
    lu->L_col_ptr[1] = 3;
    lu->U_col_ptr[1] = 4;

    cxf_lu_clear(lu);

    TEST_ASSERT_EQUAL_INT(0, lu->valid);
    TEST_ASSERT_EQUAL_INT64(0, lu->L_nnz);
    TEST_ASSERT_EQUAL_INT64(0, lu->U_nnz);
    /* Permutations should be reset to identity */
    TEST_ASSERT_EQUAL_INT(0, lu->perm_row[0]);
    TEST_ASSERT_EQUAL_INT(1, lu->perm_row[1]);
    TEST_ASSERT_EQUAL_INT(2, lu->perm_row[2]);
    /* Column pointers should be zeroed */
    TEST_ASSERT_EQUAL_INT64(0, lu->L_col_ptr[1]);
    TEST_ASSERT_EQUAL_INT64(0, lu->U_col_ptr[1]);

    cxf_lu_free(lu);
}

void test_lu_clear_null_safe(void) {
    cxf_lu_clear(NULL);  /* Should not crash */
    TEST_PASS();
}

/*******************************************************************************
 * Integration with BasisState tests
 ******************************************************************************/

void test_basis_with_lu_field(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    TEST_ASSERT_NOT_NULL(basis);
    TEST_ASSERT_NULL(basis->lu);  /* Initially NULL */

    /* Allocate LU factors manually (refactor would do this) */
    basis->lu = cxf_lu_create(3, 6, 6);
    TEST_ASSERT_NOT_NULL(basis->lu);
    TEST_ASSERT_EQUAL_INT(3, basis->lu->m);

    /* Free basis should free LU factors too */
    cxf_basis_free(basis);
    TEST_PASS();
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_lu_create tests */
    RUN_TEST(test_lu_create_returns_valid_struct);
    RUN_TEST(test_lu_create_initializes_permutations_to_identity);
    RUN_TEST(test_lu_create_zero_m_returns_null);
    RUN_TEST(test_lu_create_negative_m_returns_null);
    RUN_TEST(test_lu_create_small_nnz_estimates_adjusted);
    RUN_TEST(test_lu_create_large_dimensions);

    /* cxf_lu_free tests */
    RUN_TEST(test_lu_free_null_safe);

    /* cxf_lu_clear tests */
    RUN_TEST(test_lu_clear_resets_state);
    RUN_TEST(test_lu_clear_null_safe);

    /* Integration tests */
    RUN_TEST(test_basis_with_lu_field);

    return UNITY_END();
}
