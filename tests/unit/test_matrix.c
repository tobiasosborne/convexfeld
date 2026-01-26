/**
 * @file test_matrix.c
 * @brief TDD tests for matrix operations (M4.1.1)
 *
 * Tests for SparseMatrix operations, SpMV, dot product, and vector norms.
 * ~200 LOC of comprehensive tests.
 */

#include "unity.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/*******************************************************************************
 * External function declarations (to be implemented)
 ******************************************************************************/

/* Existing stub functions */
SparseMatrix *cxf_sparse_create(void);
void cxf_sparse_free(SparseMatrix *mat);
int cxf_sparse_init_csc(SparseMatrix *mat, int num_rows, int num_cols,
                        int64_t nnz);

/* Functions to be implemented in M4.1.3 */
void cxf_matrix_multiply(const double *x, double *y, int num_vars,
                         int num_constrs, const int64_t *col_start,
                         const int *row_indices, const double *coeff_values,
                         int accumulate);

/* Functions to be implemented in M4.1.4 */
double cxf_dot_product(const double *x, const double *y, int n);
double cxf_dot_product_sparse(const int *x_indices, const double *x_values,
                              int x_nnz, const double *y_dense);
double cxf_vector_norm(const double *x, int n, int norm_type);

/* Functions to be implemented in M4.1.5 */
int cxf_prepare_row_data(SparseMatrix *mat);
int cxf_build_row_major(SparseMatrix *mat);
int cxf_finalize_row_data(SparseMatrix *mat);

/* Functions to be implemented in M4.1.6 */
void cxf_sort_indices(int *indices, int n);
void cxf_sort_indices_values(int *indices, double *values, int n);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * SparseMatrix creation/free tests (already implemented in stub)
 ******************************************************************************/

void test_sparse_create_returns_valid_matrix(void) {
    SparseMatrix *mat = cxf_sparse_create();
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
    SparseMatrix *mat = cxf_sparse_create();
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
    SparseMatrix *mat = cxf_sparse_create();
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
 * cxf_dot_product tests
 ******************************************************************************/

void test_dot_product_basic(void) {
    double x[] = {1.0, 2.0, 3.0};
    double y[] = {4.0, 5.0, 6.0};

    /* Expected: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32 */
    double result = cxf_dot_product(x, y, 3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 32.0, result);
}

void test_dot_product_single_element(void) {
    double x[] = {5.0};
    double y[] = {3.0};

    double result = cxf_dot_product(x, y, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 15.0, result);
}

void test_dot_product_orthogonal(void) {
    double x[] = {1.0, 0.0, 0.0};
    double y[] = {0.0, 1.0, 0.0};

    double result = cxf_dot_product(x, y, 3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result);
}

void test_dot_product_self(void) {
    double x[] = {3.0, 4.0};  /* 3^2 + 4^2 = 25 */

    double result = cxf_dot_product(x, x, 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 25.0, result);
}

void test_dot_product_sparse_dense(void) {
    int x_indices[] = {0, 2, 4};
    double x_values[] = {1.0, 2.0, 3.0};
    double y_dense[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    /* Expected: 1*10 + 2*30 + 3*50 = 10 + 60 + 150 = 220 */
    double result = cxf_dot_product_sparse(x_indices, x_values, 3, y_dense);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 220.0, result);
}

void test_dot_product_sparse_empty(void) {
    double y_dense[] = {10.0, 20.0, 30.0};

    double result = cxf_dot_product_sparse(NULL, NULL, 0, y_dense);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result);
}

/*******************************************************************************
 * cxf_vector_norm tests
 ******************************************************************************/

void test_vector_norm_l1(void) {
    double x[] = {1.0, -2.0, 3.0, -4.0};

    /* L1 = |1| + |-2| + |3| + |-4| = 10 */
    double result = cxf_vector_norm(x, 4, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 10.0, result);
}

void test_vector_norm_l2(void) {
    double x[] = {3.0, 4.0};

    /* L2 = sqrt(9 + 16) = 5 */
    double result = cxf_vector_norm(x, 2, 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, result);
}

void test_vector_norm_linf(void) {
    double x[] = {1.0, -5.0, 3.0, -2.0};

    /* L∞ = max(|1|, |-5|, |3|, |-2|) = 5 */
    double result = cxf_vector_norm(x, 4, 0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, result);
}

void test_vector_norm_zero_vector(void) {
    double x[] = {0.0, 0.0, 0.0};

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 2));
}

void test_vector_norm_single_element(void) {
    double x[] = {-7.0};

    /* All norms of single element = |element| */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 2));
}

/*******************************************************************************
 * Row-major conversion tests (M4.1.5)
 ******************************************************************************/

void test_row_major_full_pipeline(void) {
    /* Create 2x3 matrix: A = [[1, 2, 0], [3, 0, 4]] in CSC */
    SparseMatrix *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 2, 3, 4);

    /* CSC: col 0 has [1,3], col 1 has [2], col 2 has [4] */
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 2; mat->col_ptr[2] = 3; mat->col_ptr[3] = 4;
    mat->row_idx[0] = 0; mat->row_idx[1] = 1; mat->row_idx[2] = 0; mat->row_idx[3] = 1;
    mat->values[0] = 1.0; mat->values[1] = 3.0; mat->values[2] = 2.0; mat->values[3] = 4.0;

    /* Run 3-stage pipeline */
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_prepare_row_data(mat));
    TEST_ASSERT_NOT_NULL(mat->row_ptr);

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_build_row_major(mat));

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_finalize_row_data(mat));

    /* Verify CSR: row 0 has [1,2] at cols [0,1], row 1 has [3,4] at cols [0,2] */
    TEST_ASSERT_EQUAL_INT64(0, mat->row_ptr[0]);
    TEST_ASSERT_EQUAL_INT64(2, mat->row_ptr[1]);
    TEST_ASSERT_EQUAL_INT64(4, mat->row_ptr[2]);

    /* Row 0 entries */
    TEST_ASSERT_EQUAL_INT(0, mat->col_idx[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, mat->row_values[0]);
    TEST_ASSERT_EQUAL_INT(1, mat->col_idx[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, mat->row_values[1]);

    /* Row 1 entries */
    TEST_ASSERT_EQUAL_INT(0, mat->col_idx[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, mat->row_values[2]);
    TEST_ASSERT_EQUAL_INT(2, mat->col_idx[3]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, mat->row_values[3]);

    cxf_sparse_free(mat);
}

void test_prepare_row_data_null_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_prepare_row_data(NULL));
}

void test_build_row_major_without_prepare_returns_error(void) {
    SparseMatrix *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 2, 2, 1);
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 1; mat->col_ptr[2] = 1;
    mat->row_idx[0] = 0; mat->values[0] = 1.0;

    /* row_ptr is NULL since prepare wasn't called */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_build_row_major(mat));

    cxf_sparse_free(mat);
}

void test_row_major_empty_matrix(void) {
    SparseMatrix *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 3, 3, 0);
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 0; mat->col_ptr[2] = 0; mat->col_ptr[3] = 0;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_prepare_row_data(mat));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_build_row_major(mat));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_finalize_row_data(mat));

    /* All row pointers should be 0 */
    for (int i = 0; i <= 3; i++) {
        TEST_ASSERT_EQUAL_INT64(0, mat->row_ptr[i]);
    }

    cxf_sparse_free(mat);
}

/*******************************************************************************
 * Sort indices tests (M4.1.6)
 ******************************************************************************/

void test_sort_indices_basic(void) {
    int indices[] = {5, 2, 8, 1, 9};
    cxf_sort_indices(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(2, indices[1]);
    TEST_ASSERT_EQUAL_INT(5, indices[2]);
    TEST_ASSERT_EQUAL_INT(8, indices[3]);
    TEST_ASSERT_EQUAL_INT(9, indices[4]);
}

void test_sort_indices_already_sorted(void) {
    int indices[] = {1, 2, 3, 4, 5};
    cxf_sort_indices(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(5, indices[4]);
}

void test_sort_indices_reverse(void) {
    int indices[] = {5, 4, 3, 2, 1};
    cxf_sort_indices(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(2, indices[1]);
    TEST_ASSERT_EQUAL_INT(5, indices[4]);
}

void test_sort_indices_single(void) {
    int indices[] = {42};
    cxf_sort_indices(indices, 1);
    TEST_ASSERT_EQUAL_INT(42, indices[0]);

    /* Test empty */
    cxf_sort_indices(NULL, 0);
    TEST_PASS();
}

void test_sort_indices_values_sync(void) {
    int indices[] = {3, 1, 2};
    double values[] = {30.0, 10.0, 20.0};

    cxf_sort_indices_values(indices, values, 3);

    /* Indices sorted: 1, 2, 3 */
    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(2, indices[1]);
    TEST_ASSERT_EQUAL_INT(3, indices[2]);

    /* Values follow their original indices */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, values[0]);  /* Was at index 1 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, values[1]);  /* Was at index 2 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 30.0, values[2]);  /* Was at index 3 */
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* SparseMatrix tests (stub already implemented) */
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

    /* Dot product tests (to be implemented) */
    RUN_TEST(test_dot_product_basic);
    RUN_TEST(test_dot_product_single_element);
    RUN_TEST(test_dot_product_orthogonal);
    RUN_TEST(test_dot_product_self);
    RUN_TEST(test_dot_product_sparse_dense);
    RUN_TEST(test_dot_product_sparse_empty);

    /* Vector norm tests (to be implemented) */
    RUN_TEST(test_vector_norm_l1);
    RUN_TEST(test_vector_norm_l2);
    RUN_TEST(test_vector_norm_linf);
    RUN_TEST(test_vector_norm_zero_vector);
    RUN_TEST(test_vector_norm_single_element);

    /* Row-major conversion tests (M4.1.5) */
    RUN_TEST(test_row_major_full_pipeline);
    RUN_TEST(test_prepare_row_data_null_returns_error);
    RUN_TEST(test_build_row_major_without_prepare_returns_error);
    RUN_TEST(test_row_major_empty_matrix);

    /* Sort indices tests (M4.1.6) */
    RUN_TEST(test_sort_indices_basic);
    RUN_TEST(test_sort_indices_already_sorted);
    RUN_TEST(test_sort_indices_reverse);
    RUN_TEST(test_sort_indices_single);
    RUN_TEST(test_sort_indices_values_sync);

    return UNITY_END();
}
