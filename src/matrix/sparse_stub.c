/**
 * @file sparse_stub.c
 * @brief Stub implementation for SparseMatrix operations.
 *
 * Provides minimal stubs for creating and freeing sparse matrices.
 * Full implementation comes in M4.1 (Matrix Operations).
 */

#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Allocate and initialize an empty SparseMatrix.
 *
 * Creates a SparseMatrix with zero dimensions. Arrays are not allocated
 * until matrix is populated.
 *
 * @return Pointer to new SparseMatrix, or NULL on allocation failure.
 */
SparseMatrix *cxf_sparse_create(void) {
    SparseMatrix *mat = (SparseMatrix *)calloc(1, sizeof(SparseMatrix));
    if (mat == NULL) {
        return NULL;
    }

    /* All fields zeroed by calloc - pointers are NULL, dimensions are 0 */
    return mat;
}

/**
 * @brief Free a SparseMatrix and all its arrays.
 *
 * Frees all allocated arrays (CSC and CSR formats) and the structure itself.
 * Safe to call with NULL pointer.
 *
 * @param mat Matrix to free (may be NULL).
 */
void cxf_sparse_free(SparseMatrix *mat) {
    if (mat == NULL) {
        return;
    }

    /* Free CSC format arrays */
    free(mat->col_ptr);
    free(mat->row_idx);
    free(mat->values);

    /* Free CSR format arrays (if built) */
    free(mat->row_ptr);
    free(mat->col_idx);
    free(mat->row_values);

    /* Free constraint data */
    free(mat->rhs);
    free(mat->sense);

    /* Free the structure itself */
    free(mat);
}

/**
 * @brief Initialize CSC arrays for a SparseMatrix.
 *
 * Allocates col_ptr, row_idx, and values arrays based on dimensions.
 * Does not populate values - caller must fill arrays.
 *
 * @param mat Matrix to initialize.
 * @param num_rows Number of rows (constraints).
 * @param num_cols Number of columns (variables).
 * @param nnz Number of non-zero entries.
 * @return CXF_OK on success, error code on failure.
 */
int cxf_sparse_init_csc(SparseMatrix *mat, int num_rows, int num_cols,
                        int64_t nnz) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (num_rows < 0 || num_cols < 0 || nnz < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    mat->num_rows = num_rows;
    mat->num_cols = num_cols;
    mat->nnz = nnz;

    /* Allocate column pointers (num_cols + 1) */
    mat->col_ptr = (int64_t *)calloc((size_t)(num_cols + 1), sizeof(int64_t));
    if (mat->col_ptr == NULL && num_cols >= 0) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate row indices and values (nnz each) */
    if (nnz > 0) {
        mat->row_idx = (int *)calloc((size_t)nnz, sizeof(int));
        mat->values = (double *)calloc((size_t)nnz, sizeof(double));

        if (mat->row_idx == NULL || mat->values == NULL) {
            free(mat->col_ptr);
            free(mat->row_idx);
            free(mat->values);
            mat->col_ptr = NULL;
            mat->row_idx = NULL;
            mat->values = NULL;
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    return CXF_OK;
}

/*******************************************************************************
 * TDD Stubs - To be implemented in M4.1.4
 ******************************************************************************/

/* cxf_matrix_multiply implemented in multiply.c (M4.1.3) */

/**
 * @brief Dense dot product stub (M4.1.4).
 * @note Full implementation in src/matrix/vectors.c
 */
double cxf_dot_product(const double *x, const double *y, int n) {
    (void)x; (void)y; (void)n;
    return 0.0;  /* Stub - tests will fail */
}

/**
 * @brief Sparse-dense dot product stub (M4.1.4).
 * @note Full implementation in src/matrix/vectors.c
 */
double cxf_dot_product_sparse(const int *x_indices, const double *x_values,
                              int x_nnz, const double *y_dense) {
    (void)x_indices; (void)x_values; (void)x_nnz; (void)y_dense;
    return 0.0;  /* Stub - tests will fail */
}

/**
 * @brief Vector norm stub (M4.1.4).
 * @note Full implementation in src/matrix/vectors.c
 */
double cxf_vector_norm(const double *x, int n, int norm_type) {
    (void)x; (void)n; (void)norm_type;
    return 0.0;  /* Stub - tests will fail */
}
