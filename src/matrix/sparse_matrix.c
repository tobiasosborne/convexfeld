/**
 * @file sparse_matrix.c
 * @brief Full SparseMatrix implementation (M4.1.2)
 *
 * Implements CSC validation, CSR construction from CSC, and related utilities.
 * Creation/destruction functions are in sparse_stub.c.
 *
 * ~150 LOC target
 */

#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Validate CSC structure invariants.
 *
 * Checks:
 * - col_ptr is monotonically non-decreasing
 * - col_ptr[0] == 0
 * - col_ptr[num_cols] == nnz
 * - All row indices are in range [0, num_rows)
 *
 * @param mat Matrix to validate.
 * @return CXF_OK if valid, error code otherwise.
 */
int cxf_sparse_validate(const SparseMatrix *mat) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Empty matrix is valid */
    if (mat->num_cols == 0 && mat->num_rows == 0 && mat->nnz == 0) {
        return CXF_OK;
    }

    /* Validate dimensions */
    if (mat->num_rows < 0 || mat->num_cols < 0 || mat->nnz < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Must have col_ptr if num_cols > 0 */
    if (mat->num_cols > 0 && mat->col_ptr == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate col_ptr starts at 0 */
    if (mat->col_ptr != NULL && mat->col_ptr[0] != 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate col_ptr ends at nnz */
    if (mat->col_ptr != NULL && mat->col_ptr[mat->num_cols] != mat->nnz) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate col_ptr is monotonically non-decreasing */
    for (int j = 0; j < mat->num_cols; j++) {
        if (mat->col_ptr[j] > mat->col_ptr[j + 1]) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    /* Must have row_idx and values if nnz > 0 */
    if (mat->nnz > 0 && (mat->row_idx == NULL || mat->values == NULL)) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate all row indices are in range */
    for (int64_t k = 0; k < mat->nnz; k++) {
        if (mat->row_idx[k] < 0 || mat->row_idx[k] >= mat->num_rows) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    return CXF_OK;
}

/**
 * @brief Build CSR format from existing CSC format.
 *
 * Converts the CSC representation to CSR for efficient row access.
 * Allocates row_ptr, col_idx, and row_values arrays.
 *
 * @param mat Matrix with valid CSC format (will have CSR added).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_sparse_build_csr(SparseMatrix *mat) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Validate CSC first */
    int status = cxf_sparse_validate(mat);
    if (status != CXF_OK) {
        return status;
    }

    /* Free existing CSR if any */
    free(mat->row_ptr);
    free(mat->col_idx);
    free(mat->row_values);
    mat->row_ptr = NULL;
    mat->col_idx = NULL;
    mat->row_values = NULL;

    /* Empty matrix - nothing to do */
    if (mat->nnz == 0) {
        /* Allocate minimal row_ptr */
        mat->row_ptr = (int64_t *)calloc((size_t)(mat->num_rows + 1),
                                         sizeof(int64_t));
        if (mat->row_ptr == NULL && mat->num_rows >= 0) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        return CXF_OK;
    }

    /* Allocate CSR arrays */
    mat->row_ptr = (int64_t *)calloc((size_t)(mat->num_rows + 1),
                                     sizeof(int64_t));
    mat->col_idx = (int *)calloc((size_t)mat->nnz, sizeof(int));
    mat->row_values = (double *)calloc((size_t)mat->nnz, sizeof(double));

    if (mat->row_ptr == NULL || mat->col_idx == NULL ||
        mat->row_values == NULL) {
        free(mat->row_ptr);
        free(mat->col_idx);
        free(mat->row_values);
        mat->row_ptr = NULL;
        mat->col_idx = NULL;
        mat->row_values = NULL;
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Count entries per row */
    for (int64_t k = 0; k < mat->nnz; k++) {
        mat->row_ptr[mat->row_idx[k] + 1]++;
    }

    /* Convert counts to cumulative offsets */
    for (int i = 0; i < mat->num_rows; i++) {
        mat->row_ptr[i + 1] += mat->row_ptr[i];
    }

    /* Fill CSR arrays using a working copy of row_ptr */
    int64_t *work = (int64_t *)malloc((size_t)mat->num_rows * sizeof(int64_t));
    if (work == NULL) {
        free(mat->row_ptr);
        free(mat->col_idx);
        free(mat->row_values);
        mat->row_ptr = NULL;
        mat->col_idx = NULL;
        mat->row_values = NULL;
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    memcpy(work, mat->row_ptr, (size_t)mat->num_rows * sizeof(int64_t));

    /* Transpose: iterate through CSC and place in CSR */
    for (int j = 0; j < mat->num_cols; j++) {
        for (int64_t k = mat->col_ptr[j]; k < mat->col_ptr[j + 1]; k++) {
            int row = mat->row_idx[k];
            int64_t dest = work[row]++;
            mat->col_idx[dest] = j;
            mat->row_values[dest] = mat->values[k];
        }
    }

    free(work);
    return CXF_OK;
}

/**
 * @brief Free CSR arrays only (keep CSC).
 *
 * Useful after matrix modification to invalidate cached CSR.
 *
 * @param mat Matrix to modify.
 */
void cxf_sparse_free_csr(SparseMatrix *mat) {
    if (mat == NULL) {
        return;
    }

    free(mat->row_ptr);
    free(mat->col_idx);
    free(mat->row_values);
    mat->row_ptr = NULL;
    mat->col_idx = NULL;
    mat->row_values = NULL;
}
