/**
 * @file test_matrix_convert.c
 * @brief TDD tests for row-major conversion and sort operations (M4.1.5, M4.1.6)
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

/* Functions to be implemented in M4.1.5 */
int cxf_prepare_row_data(MatrixData *mat);
int cxf_build_row_major(MatrixData *mat);

/* Functions to be implemented in M4.1.6 */
void cxf_sort_by_values(int *indices, int n);
void cxf_sort_by_values_paired(int *indices, double *values, int n);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * Row-major conversion tests (M4.1.5)
 ******************************************************************************/

void test_row_major_full_pipeline(void) {
    /* Create 2x3 matrix: A = [[1, 2, 0], [3, 0, 4]] in CSC */
    MatrixData *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 2, 3, 4);

    /* CSC: col 0 has [1,3], col 1 has [2], col 2 has [4] */
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 2; mat->col_ptr[2] = 3; mat->col_ptr[3] = 4;
    mat->row_idx[0] = 0; mat->row_idx[1] = 1; mat->row_idx[2] = 0; mat->row_idx[3] = 1;
    mat->values[0] = 1.0; mat->values[1] = 3.0; mat->values[2] = 2.0; mat->values[3] = 4.0;

    /* Run 3-stage pipeline */
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_prepare_row_data(mat));
    TEST_ASSERT_NOT_NULL(mat->row_ptr);

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_build_row_major(mat));

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
    MatrixData *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 2, 2, 1);
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 1; mat->col_ptr[2] = 1;
    mat->row_idx[0] = 0; mat->values[0] = 1.0;

    /* row_ptr is NULL since prepare wasn't called */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_build_row_major(mat));

    cxf_sparse_free(mat);
}

void test_row_major_empty_matrix(void) {
    MatrixData *mat = cxf_sparse_create();
    cxf_sparse_init_csc(mat, 3, 3, 0);
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 0; mat->col_ptr[2] = 0; mat->col_ptr[3] = 0;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_prepare_row_data(mat));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_build_row_major(mat));

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
    cxf_sort_by_values(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(2, indices[1]);
    TEST_ASSERT_EQUAL_INT(5, indices[2]);
    TEST_ASSERT_EQUAL_INT(8, indices[3]);
    TEST_ASSERT_EQUAL_INT(9, indices[4]);
}

void test_sort_indices_already_sorted(void) {
    int indices[] = {1, 2, 3, 4, 5};
    cxf_sort_by_values(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(5, indices[4]);
}

void test_sort_indices_reverse(void) {
    int indices[] = {5, 4, 3, 2, 1};
    cxf_sort_by_values(indices, 5);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(2, indices[1]);
    TEST_ASSERT_EQUAL_INT(5, indices[4]);
}

void test_sort_indices_single(void) {
    int indices[] = {42};
    cxf_sort_by_values(indices, 1);
    TEST_ASSERT_EQUAL_INT(42, indices[0]);

    /* Test empty */
    cxf_sort_by_values(NULL, 0);
    TEST_PASS();
}

void test_sort_indices_values_sync(void) {
    int indices[] = {3, 1, 2};
    double values[] = {30.0, 10.0, 20.0};

    cxf_sort_by_values_paired(indices, values, 3);

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
