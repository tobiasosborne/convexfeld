/**
 * @file row_major.c
 * @brief CSR (row-major) format construction (M4.1.5)
 *
 * Implements the 3-stage row-major conversion pipeline:
 * 1. cxf_prepare_row_data - Validate CSC and allocate CSR arrays
 * 2. cxf_build_row_major - Fill CSR arrays via transpose
 * 3. cxf_finalize_row_data - Mark conversion complete
 *
 * Spec: docs/specs/functions/matrix/cxf_build_row_major.md
 */

#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/* Forward declaration */
int cxf_sparse_validate(const SparseMatrix *mat);

/*============================================================================
 * Stage 1: Prepare Row Data (Allocate CSR Arrays)
 *===========================================================================*/

/**
 * @brief Prepare CSR arrays for row-major conversion.
 *
 * Validates CSC structure and allocates CSR arrays (row_ptr, col_idx, row_values).
 * Must be called before cxf_build_row_major.
 *
 * @param mat Matrix with valid CSC format
 * @return CXF_OK on success, error code on failure
 */
int cxf_prepare_row_data(SparseMatrix *mat) {
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

    /* Allocate row_ptr (always needed, even for empty matrix) */
    mat->row_ptr = (int64_t *)calloc((size_t)(mat->num_rows + 1), sizeof(int64_t));
    if (mat->row_ptr == NULL && mat->num_rows >= 0) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate col_idx and row_values if nnz > 0 */
    if (mat->nnz > 0) {
        mat->col_idx = (int *)calloc((size_t)mat->nnz, sizeof(int));
        mat->row_values = (double *)calloc((size_t)mat->nnz, sizeof(double));

        if (mat->col_idx == NULL || mat->row_values == NULL) {
            free(mat->row_ptr);
            free(mat->col_idx);
            free(mat->row_values);
            mat->row_ptr = NULL;
            mat->col_idx = NULL;
            mat->row_values = NULL;
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    return CXF_OK;
}

/*============================================================================
 * Stage 2: Build Row Major (Fill CSR Arrays)
 *===========================================================================*/

/**
 * @brief Build CSR format from CSC via transpose.
 *
 * Two-pass algorithm:
 * 1. Count entries per row, compute cumulative offsets
 * 2. Fill CSR arrays (reverse column order for sorted output)
 *
 * Precondition: cxf_prepare_row_data must have been called.
 *
 * @param mat Matrix with prepared CSR arrays
 * @return CXF_OK on success, error code on failure
 */
int cxf_build_row_major(SparseMatrix *mat) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (mat->row_ptr == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;  /* prepare not called */
    }

    /* Empty matrix - row_ptr already zeroed by calloc */
    if (mat->nnz == 0) {
        return CXF_OK;
    }

    /* Pass 1: Count entries per row */
    for (int64_t k = 0; k < mat->nnz; k++) {
        mat->row_ptr[mat->row_idx[k] + 1]++;
    }

    /* Convert counts to cumulative offsets */
    for (int i = 0; i < mat->num_rows; i++) {
        mat->row_ptr[i + 1] += mat->row_ptr[i];
    }

    /* Allocate working copy of row_ptr */
    int64_t *work = (int64_t *)malloc((size_t)mat->num_rows * sizeof(int64_t));
    if (work == NULL && mat->num_rows > 0) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    if (mat->num_rows > 0) {
        memcpy(work, mat->row_ptr, (size_t)mat->num_rows * sizeof(int64_t));
    }

    /* Pass 2: Fill CSR arrays */
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

/*============================================================================
 * Stage 3: Finalize Row Data
 *===========================================================================*/

/**
 * @brief Finalize row-major conversion.
 *
 * Marks the CSR format as complete and valid.
 * Currently a no-op since state is implicit (row_ptr != NULL).
 *
 * @param mat Matrix with built CSR format
 * @return CXF_OK on success, error code on failure
 */
int cxf_finalize_row_data(SparseMatrix *mat) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Verify CSR was built */
    if (mat->row_ptr == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Currently a no-op - could add validation or state flags in future */
    return CXF_OK;
}
