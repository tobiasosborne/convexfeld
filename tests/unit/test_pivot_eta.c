/**
 * @file test_pivot_eta.c
 * @brief Tests for cxf_pivot_with_eta function.
 *
 * Comprehensive tests for the Product Form of Inverse pivot update.
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
                       int enteringVar, int leavingVar);
void cxf_eta_free(EtaFactors *eta);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

static BasisState *test_basis = NULL;

void setUp(void) {
    test_basis = cxf_basis_create(3, 6);  /* 3 constraints, 6 variables */
    TEST_ASSERT_NOT_NULL(test_basis);
    /* Initialize basic_vars to slack variables (3, 4, 5) */
    test_basis->basic_vars[0] = 3;
    test_basis->basic_vars[1] = 4;
    test_basis->basic_vars[2] = 5;
    /* Initialize var_status: 0-2 nonbasic at lower, 3-5 basic */
    for (int j = 0; j < 3; j++) {
        test_basis->var_status[j] = -1;  /* Nonbasic at lower bound */
    }
    test_basis->var_status[3] = 0;  /* Basic in row 0 */
    test_basis->var_status[4] = 1;  /* Basic in row 1 */
    test_basis->var_status[5] = 2;  /* Basic in row 2 */
}

void tearDown(void) {
    if (test_basis != NULL) {
        cxf_basis_free(test_basis);
        test_basis = NULL;
    }
}

/*******************************************************************************
 * NULL argument tests
 ******************************************************************************/

void test_pivot_eta_null_basis_returns_error(void) {
    double pivotCol[] = {1.0, 0.5, 0.2};
    int result = cxf_pivot_with_eta(NULL, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_pivot_eta_null_column_returns_error(void) {
    int result = cxf_pivot_with_eta(test_basis, 0, NULL, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

/*******************************************************************************
 * Invalid pivot row tests
 ******************************************************************************/

void test_pivot_eta_negative_row_returns_error(void) {
    double pivotCol[] = {1.0, 0.5, 0.2};
    int result = cxf_pivot_with_eta(test_basis, -1, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_pivot_eta_row_too_large_returns_error(void) {
    double pivotCol[] = {1.0, 0.5, 0.2};
    int result = cxf_pivot_with_eta(test_basis, 3, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_pivot_eta_row_equals_m_returns_error(void) {
    double pivotCol[] = {1.0, 0.5, 0.2};
    int result = cxf_pivot_with_eta(test_basis, test_basis->m, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*******************************************************************************
 * Pivot element tests
 ******************************************************************************/

void test_pivot_eta_zero_pivot_returns_minus_one(void) {
    double pivotCol[] = {0.0, 0.5, 0.2};  /* Pivot element is 0 */
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_pivot_eta_tiny_pivot_returns_minus_one(void) {
    double pivotCol[] = {1e-15, 0.5, 0.2};  /* Below CXF_PIVOT_TOL */
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_pivot_eta_pivot_at_tolerance_returns_minus_one(void) {
    double pivotCol[] = {CXF_PIVOT_TOL * 0.5, 0.5, 0.2};  /* Just below tolerance */
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_pivot_eta_negative_large_pivot_succeeds(void) {
    double pivotCol[] = {-1.0, 0.5, 0.2};  /* Large negative pivot */
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

/*******************************************************************************
 * Successful pivot tests
 ******************************************************************************/

void test_pivot_eta_basic_pivot_succeeds(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_pivot_eta_updates_basic_vars(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int enteringVar = 0;
    int leavingVar = 3;  /* Was basic in row 0 */

    cxf_pivot_with_eta(test_basis, 0, pivotCol, enteringVar, leavingVar);

    TEST_ASSERT_EQUAL_INT(enteringVar, test_basis->basic_vars[0]);
    /* Other basic vars unchanged */
    TEST_ASSERT_EQUAL_INT(4, test_basis->basic_vars[1]);
    TEST_ASSERT_EQUAL_INT(5, test_basis->basic_vars[2]);
}

void test_pivot_eta_updates_var_status(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int enteringVar = 0;
    int leavingVar = 3;

    cxf_pivot_with_eta(test_basis, 0, pivotCol, enteringVar, leavingVar);

    TEST_ASSERT_EQUAL_INT(0, test_basis->var_status[enteringVar]);  /* Basic in row 0 */
    TEST_ASSERT_EQUAL_INT(-1, test_basis->var_status[leavingVar]);  /* Nonbasic lower */
}

void test_pivot_eta_increments_eta_count(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int initial_count = test_basis->eta_count;

    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    TEST_ASSERT_EQUAL_INT(initial_count + 1, test_basis->eta_count);
}

void test_pivot_eta_increments_pivots_since_refactor(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int initial_pivots = test_basis->pivots_since_refactor;

    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    TEST_ASSERT_EQUAL_INT(initial_pivots + 1, test_basis->pivots_since_refactor);
}

void test_pivot_eta_creates_eta_head(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    TEST_ASSERT_NULL(test_basis->eta_head);

    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    TEST_ASSERT_NOT_NULL(test_basis->eta_head);
}

void test_pivot_eta_sets_eta_type_to_2(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    TEST_ASSERT_EQUAL_INT(2, test_basis->eta_head->type);
}

void test_pivot_eta_sets_eta_pivot_row(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(test_basis, 1, pivotCol, 0, 4);

    TEST_ASSERT_EQUAL_INT(1, test_basis->eta_head->pivot_row);
}

void test_pivot_eta_sets_eta_pivot_var(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    int enteringVar = 2;
    cxf_pivot_with_eta(test_basis, 0, pivotCol, enteringVar, 3);

    TEST_ASSERT_EQUAL_INT(enteringVar, test_basis->eta_head->pivot_var);
}

void test_pivot_eta_sets_eta_multiplier(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    /* pivot_elem = pivot value (raw) = 2.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, test_basis->eta_head->pivot_elem);
}

/*******************************************************************************
 * Eta vector correctness tests
 ******************************************************************************/

void test_pivot_eta_sparse_column_counts_nnz(void) {
    /* pivotCol[0] = 2.0 (pivot), pivotCol[1] = 0.5, pivotCol[2] near zero */
    double pivotCol[] = {2.0, 0.5, 1e-14};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    /* nnz = 1 (only row 1 is nonzero, excluding pivot row) */
    TEST_ASSERT_EQUAL_INT(1, test_basis->eta_head->nnz);
}

void test_pivot_eta_dense_column_counts_nnz(void) {
    double pivotCol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    /* nnz = 2 (rows 1 and 2 are nonzero, excluding pivot row 0) */
    TEST_ASSERT_EQUAL_INT(2, test_basis->eta_head->nnz);
}

void test_pivot_eta_computes_eta_values(void) {
    double pivotCol[] = {2.0, 0.6, 0.4};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    /* Values are stored raw (unscaled, positive):
     * eta[1] = 0.6 (raw column value)
     * eta[2] = 0.4 (raw column value) */
    EtaFactors *eta = test_basis->eta_head;
    TEST_ASSERT_EQUAL_INT(2, eta->nnz);
    TEST_ASSERT_EQUAL_INT(1, eta->indices[0]);
    TEST_ASSERT_EQUAL_INT(2, eta->indices[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.6, eta->values[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.4, eta->values[1]);
}

void test_pivot_eta_identity_column_has_zero_nnz(void) {
    /* Only pivot element nonzero - identity column */
    double pivotCol[] = {1.0, 0.0, 0.0};
    cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);

    TEST_ASSERT_EQUAL_INT(0, test_basis->eta_head->nnz);
    TEST_ASSERT_NULL(test_basis->eta_head->indices);
    TEST_ASSERT_NULL(test_basis->eta_head->values);
}

/*******************************************************************************
 * Boundary condition tests
 ******************************************************************************/

void test_pivot_eta_first_row_pivot(void) {
    double pivotCol[] = {1.5, 0.3, 0.1};
    int result = cxf_pivot_with_eta(test_basis, 0, pivotCol, 0, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_EQUAL_INT(0, test_basis->basic_vars[0]);
}

void test_pivot_eta_last_row_pivot(void) {
    double pivotCol[] = {0.1, 0.3, 1.5};
    int result = cxf_pivot_with_eta(test_basis, 2, pivotCol, 0, 5);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_EQUAL_INT(0, test_basis->basic_vars[2]);
}

void test_pivot_eta_middle_row_pivot(void) {
    double pivotCol[] = {0.1, 1.5, 0.3};
    int result = cxf_pivot_with_eta(test_basis, 1, pivotCol, 0, 4);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_EQUAL_INT(0, test_basis->basic_vars[1]);
}

/*******************************************************************************
 * Multiple pivots test
 ******************************************************************************/

void test_pivot_eta_multiple_pivots_chain_etas(void) {
    double pivotCol1[] = {2.0, 0.5, 0.25};
    double pivotCol2[] = {0.1, 1.5, 0.3};

    cxf_pivot_with_eta(test_basis, 0, pivotCol1, 0, 3);
    cxf_pivot_with_eta(test_basis, 1, pivotCol2, 1, 4);

    TEST_ASSERT_EQUAL_INT(2, test_basis->eta_count);
    /* New eta is prepended to head */
    TEST_ASSERT_EQUAL_INT(1, test_basis->eta_head->pivot_row);
    TEST_ASSERT_NOT_NULL(test_basis->eta_head->next);
    TEST_ASSERT_EQUAL_INT(0, test_basis->eta_head->next->pivot_row);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* NULL argument tests */
    RUN_TEST(test_pivot_eta_null_basis_returns_error);
    RUN_TEST(test_pivot_eta_null_column_returns_error);

    /* Invalid pivot row tests */
    RUN_TEST(test_pivot_eta_negative_row_returns_error);
    RUN_TEST(test_pivot_eta_row_too_large_returns_error);
    RUN_TEST(test_pivot_eta_row_equals_m_returns_error);

    /* Pivot element tests */
    RUN_TEST(test_pivot_eta_zero_pivot_returns_minus_one);
    RUN_TEST(test_pivot_eta_tiny_pivot_returns_minus_one);
    RUN_TEST(test_pivot_eta_pivot_at_tolerance_returns_minus_one);
    RUN_TEST(test_pivot_eta_negative_large_pivot_succeeds);

    /* Successful pivot tests */
    RUN_TEST(test_pivot_eta_basic_pivot_succeeds);
    RUN_TEST(test_pivot_eta_updates_basic_vars);
    RUN_TEST(test_pivot_eta_updates_var_status);
    RUN_TEST(test_pivot_eta_increments_eta_count);
    RUN_TEST(test_pivot_eta_increments_pivots_since_refactor);
    RUN_TEST(test_pivot_eta_creates_eta_head);
    RUN_TEST(test_pivot_eta_sets_eta_type_to_2);
    RUN_TEST(test_pivot_eta_sets_eta_pivot_row);
    RUN_TEST(test_pivot_eta_sets_eta_pivot_var);
    RUN_TEST(test_pivot_eta_sets_eta_multiplier);

    /* Eta vector correctness tests */
    RUN_TEST(test_pivot_eta_sparse_column_counts_nnz);
    RUN_TEST(test_pivot_eta_dense_column_counts_nnz);
    RUN_TEST(test_pivot_eta_computes_eta_values);
    RUN_TEST(test_pivot_eta_identity_column_has_zero_nnz);

    /* Boundary condition tests */
    RUN_TEST(test_pivot_eta_first_row_pivot);
    RUN_TEST(test_pivot_eta_last_row_pivot);
    RUN_TEST(test_pivot_eta_middle_row_pivot);

    /* Multiple pivots test */
    RUN_TEST(test_pivot_eta_multiple_pivots_chain_etas);

    return UNITY_END();
}
