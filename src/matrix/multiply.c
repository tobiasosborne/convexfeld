/**
 * @file multiply.c
 * @brief Sparse matrix-vector multiplication (M4.1.3)
 *
 * Implements CSC-format sparse matrix-vector multiplication: y = Ax or y += Ax.
 * This is a fundamental operation used throughout the simplex method.
 */

#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

/**
 * @brief Sparse matrix-vector multiply: y = Ax or y += Ax
 *
 * Performs sparse matrix-vector multiplication using CSC (Compressed Sparse
 * Column) format. Iterates over columns, accumulating contributions to output.
 * Skips columns where x[j] = 0 for efficiency with sparse vectors.
 *
 * @param x Input vector (length num_vars).
 * @param y Output vector (length num_constrs), modified in place.
 * @param num_vars Number of variables (columns in matrix).
 * @param num_constrs Number of constraints (rows in matrix).
 * @param col_start CSC column pointers (length num_vars + 1).
 * @param row_indices CSC row indices (length nnz).
 * @param coeff_values CSC coefficient values (length nnz).
 * @param accumulate 0 = overwrite y with Ax, 1 = add Ax to existing y.
 *
 * @note No explicit error checking for performance. Caller must ensure:
 *       - All pointers are valid
 *       - Arrays have correct sizes
 *       - row_indices values are in range [0, num_constrs)
 */
void cxf_matrix_multiply(const double *x, double *y, int num_vars,
                         int num_constrs, const int64_t *col_start,
                         const int *row_indices, const double *coeff_values,
                         int accumulate) {
    /* Initialize output to zero if not accumulating */
    if (accumulate == 0) {
        memset(y, 0, (size_t)num_constrs * sizeof(double));
    }

    /* Iterate over columns (variables) */
    for (int j = 0; j < num_vars; j++) {
        double xj = x[j];

        /* Skip zero entries for efficiency (common in simplex) */
        if (xj == 0.0) {
            continue;
        }

        /* Get column range in CSC structure */
        int64_t start = col_start[j];
        int64_t end = col_start[j + 1];

        /* Accumulate contributions from this column */
        for (int64_t k = start; k < end; k++) {
            int row = row_indices[k];
            double coeff = coeff_values[k];
            y[row] += coeff * xj;
        }
    }
}

/**
 * @brief Sparse matrix-transpose-vector multiply: y = A^T x or y += A^T x
 *
 * Performs transpose multiplication using CSC format (which acts like CSR
 * for the transpose). Each column of A becomes a row of A^T.
 *
 * @param x Input vector (length num_constrs).
 * @param y Output vector (length num_vars), modified in place.
 * @param num_vars Number of variables (columns in A, rows in A^T).
 * @param num_constrs Number of constraints (rows in A, columns in A^T).
 * @param col_start CSC column pointers (length num_vars + 1).
 * @param row_indices CSC row indices (length nnz).
 * @param coeff_values CSC coefficient values (length nnz).
 * @param accumulate 0 = overwrite y with A^T x, 1 = add A^T x to existing y.
 */
void cxf_matrix_transpose_multiply(const double *x, double *y, int num_vars,
                                   int num_constrs, const int64_t *col_start,
                                   const int *row_indices,
                                   const double *coeff_values, int accumulate) {
    (void)num_constrs;  /* Used only for validation in debug builds */

    /* Initialize output to zero if not accumulating */
    if (accumulate == 0) {
        memset(y, 0, (size_t)num_vars * sizeof(double));
    }

    /* For A^T x, column j of A corresponds to row j of A^T */
    /* y[j] = sum over i of A[i,j] * x[i] */
    for (int j = 0; j < num_vars; j++) {
        int64_t start = col_start[j];
        int64_t end = col_start[j + 1];

        double sum = 0.0;
        for (int64_t k = start; k < end; k++) {
            int row = row_indices[k];
            double coeff = coeff_values[k];
            sum += coeff * x[row];
        }

        if (accumulate == 0) {
            y[j] = sum;
        } else {
            y[j] += sum;
        }
    }
}
