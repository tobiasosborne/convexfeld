/**
 * @file test_matrix_create.c
 * @brief TDD tests for MatrixData creation/lifecycle and SpMV (M4.1.1, M4.1.3)
 *
 * Split from test_matrix.c for 200 LOC limit.
 */

#include "unity.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/*******************************************************************************
 * External function declarations (to be implemented)
 ******************************************************************************/

/* Existing stub functions */
MatrixData *cxf_sparse_create(void);
void cxf_sparse_free(MatrixData *mat);
int cxf_sparse_init_csc(MatrixData *mat, int num_rows, int num_cols,
                        int64_t nnz);

/* Functions to be implemented in M4.1.3 */
void cxf_matrix_multiply(const double *x, double *y, int num_vars,
                         int num_constrs, const int64_t *col_start,
                         const int *row_indices, const double *coeff_values,
                         int accumulate);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * MatrixData creation/free tests (already implemented in stub)
 ******************************************************************************/

void test_sparse_create_returns_valid_matrix(void) {
    MatrixData *mat = cxf_sparse_create();
    TEST_ASSERT_NOT_NULL(mat);
    TEST_ASSERT_EQUAL_INT(0, mat->num_rows);
    TEST_ASSERT_EQUAL_INT(0, mat->num_cols);
    TEST_ASSERT_EQUAL_INT64(0, mat->nnz);
    cxf_sparse_free(mat);
}

void test_sparse_free_null_safe(void) {
    cxf_sparse_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_sparse_init_csc_basic(void) {
    MatrixData *mat = cxf_sparse_create();
    TEST_ASSERT_NOT_NULL(mat);

    int status = cxf_sparse_init_csc(mat, 3, 4, 5);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(3, mat->num_rows);
    TEST_ASSERT_EQUAL_INT(4, mat->num_cols);
    TEST_ASSERT_EQUAL_INT64(5, mat->nnz);
    TEST_ASSERT_NOT_NULL(mat->col_ptr);
    TEST_ASSERT_NOT_NULL(mat->row_idx);
    TEST_ASSERT_NOT_NULL(mat->values);

    cxf_sparse_free(mat);
}

void test_sparse_init_csc_empty_matrix(void) {
    MatrixData *mat = cxf_sparse_create();
    int status = cxf_sparse_init_csc(mat, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, mat->num_rows);
    cxf_sparse_free(mat);
}

void test_sparse_init_csc_null_arg(void) {
    int status = cxf_sparse_init_csc(NULL, 3, 4, 5);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

/*******************************************************************************
 * cxf_matrix_multiply tests (SpMV: y = Ax)
 ******************************************************************************/

void test_matrix_multiply_simple_2x2(void) {
    /* Matrix A = [[1, 2], [3, 4]] in CSC format */
    int64_t col_start[] = {0, 2, 4};
    int row_indices[] = {0, 1, 0, 1};
    double coeff_values[] = {1.0, 3.0, 2.0, 4.0};  /* Col 0: [1,3], Col 1: [2,4] */

    double x[] = {1.0, 1.0};  /* x = [1, 1] */
    double y[2] = {0.0, 0.0};

    /* Expected: y = Ax = [1+2, 3+4] = [3, 7] */
    cxf_matrix_multiply(x, y, 2, 2, col_start, row_indices, coeff_values, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, y[1]);
}

void test_matrix_multiply_accumulate_mode(void) {
    int64_t col_start[] = {0, 2, 4};
    int row_indices[] = {0, 1, 0, 1};
    double coeff_values[] = {1.0, 3.0, 2.0, 4.0};

    double x[] = {1.0, 1.0};
    double y[] = {10.0, 20.0};  /* Pre-existing values */

    /* Expected: y += Ax -> [10+3, 20+7] = [13, 27] */
    cxf_matrix_multiply(x, y, 2, 2, col_start, row_indices, coeff_values, 1);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 13.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 27.0, y[1]);
}

void test_matrix_multiply_sparse_column(void) {
    /* Matrix with empty column: A = [[1, 0], [0, 2]] */
    int64_t col_start[] = {0, 1, 2};
    int row_indices[] = {0, 1};
    double coeff_values[] = {1.0, 2.0};

    double x[] = {3.0, 4.0};
    double y[2] = {0.0, 0.0};

    /* Expected: y = [3*1, 4*2] = [3, 8] */
    cxf_matrix_multiply(x, y, 2, 2, col_start, row_indices, coeff_values, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 8.0, y[1]);
}

void test_matrix_multiply_zero_x_skipped(void) {
    int64_t col_start[] = {0, 2, 4};
    int row_indices[] = {0, 1, 0, 1};
    double coeff_values[] = {1.0, 3.0, 2.0, 4.0};

    double x[] = {0.0, 1.0};  /* First column should be skipped */
    double y[2] = {0.0, 0.0};

    /* Expected: y = [2, 4] (only column 1 contributes) */
    cxf_matrix_multiply(x, y, 2, 2, col_start, row_indices, coeff_values, 0);

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, y[1]);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* MatrixData tests (stub already implemented) */
    RUN_TEST(test_sparse_create_returns_valid_matrix);
    RUN_TEST(test_sparse_free_null_safe);
    RUN_TEST(test_sparse_init_csc_basic);
    RUN_TEST(test_sparse_init_csc_empty_matrix);
    RUN_TEST(test_sparse_init_csc_null_arg);

    /* SpMV tests (to be implemented) */
    RUN_TEST(test_matrix_multiply_simple_2x2);
    RUN_TEST(test_matrix_multiply_accumulate_mode);
    RUN_TEST(test_matrix_multiply_sparse_column);
    RUN_TEST(test_matrix_multiply_zero_x_skipped);

    return UNITY_END();
}
