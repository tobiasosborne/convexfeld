/**
 * @file cxf_matrix.h
 * @brief SparseMatrix structure - constraint matrix storage.
 *
 * Stores the constraint matrix A in CSC (Compressed Sparse Column) format
 * with optional CSR (Compressed Sparse Row) for efficient row access.
 */

#ifndef CXF_MATRIX_H
#define CXF_MATRIX_H

#include "cxf_types.h"

/**
 * @brief Sparse matrix in CSC format with optional CSR.
 *
 * Primary storage is CSC (column-major) for efficient column operations.
 * CSR (row-major) is built lazily when row access is needed.
 */
struct SparseMatrix {
    /* Dimensions */
    int num_rows;             /**< Number of rows (m) */
    int num_cols;             /**< Number of columns (n) */
    int64_t nnz;              /**< Number of non-zeros */

    /* CSC format (primary) */
    int64_t *col_ptr;         /**< Column pointers [num_cols + 1] */
    int *row_idx;             /**< Row indices [nnz] */
    double *values;           /**< Non-zero values [nnz] */

    /* CSR format (optional, built lazily) */
    int64_t *row_ptr;         /**< Row pointers [num_rows + 1] (NULL if not built) */
    int *col_idx;             /**< Column indices [nnz] (NULL if not built) */
    double *row_values;       /**< Row-major values [nnz] (NULL if not built) */

    /* Constraint data */
    double *rhs;              /**< Right-hand sides [num_rows] */
    char *sense;              /**< Constraint senses [num_rows] */
};

#endif /* CXF_MATRIX_H */
