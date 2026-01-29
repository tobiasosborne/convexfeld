/**
 * @file ftran.c
 * @brief Forward transformation (FTRAN) implementation (M5.1.4)
 *
 * Implements forward transformation to compute x = B^(-1) * a,
 * where B is the current basis matrix represented using Product Form
 * of Inverse (PFI) with eta vectors.
 *
 * Spec: docs/specs/functions/basis/cxf_ftran.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/** Maximum stack-allocated eta pointers before heap allocation */
#define MAX_STACK_ETAS 64

/**
 * @brief Apply LU forward/backward substitution.
 *
 * Solves B * x = b where B = P^T * L * U * Q (with permutations).
 * Steps: temp = P * b, L * w = temp, U * y = w, x = Q^T * y
 *
 * @param lu LUFactors structure with factorization.
 * @param m Dimension.
 * @param result Vector (modified in place).
 */
static void apply_lu_solve(const LUFactors *lu, int m, double *result) {
    /* Step 1: Permute input by row permutation: temp = P * result
     * perm_row[k] = original row that becomes position k */
    double *temp = (double *)malloc((size_t)m * sizeof(double));
    if (temp == NULL) return;  /* Fall back to eta-only on alloc failure */

    for (int k = 0; k < m; k++) {
        temp[k] = result[lu->perm_row[k]];
    }

    /* Step 2: Forward substitution L * w = temp
     * L is unit lower triangular, stored column-wise.
     * For each column k, update rows below k. */
    for (int k = 0; k < m; k++) {
        if (fabs(temp[k]) < 1e-15) continue;  /* Skip zeros */
        for (int64_t p = lu->L_col_ptr[k]; p < lu->L_col_ptr[k + 1]; p++) {
            int j = lu->L_row_idx[p];  /* Row index > k (below diagonal) */
            temp[j] -= lu->L_values[p] * temp[k];
        }
    }

    /* Step 3: Backward substitution U * y = temp
     * U is upper triangular with explicit diagonal. */
    for (int k = m - 1; k >= 0; k--) {
        /* Subtract off-diagonal contributions */
        for (int64_t p = lu->U_col_ptr[k]; p < lu->U_col_ptr[k + 1]; p++) {
            int j = lu->U_row_idx[p];  /* Step index > k (right of diagonal) */
            temp[k] -= lu->U_values[p] * temp[j];
        }
        /* Divide by diagonal */
        if (fabs(lu->U_diag[k]) > 1e-15) {
            temp[k] /= lu->U_diag[k];
        }
    }

    /* Step 4: Permute output by column permutation: result = Q^T * temp
     * perm_col[k] = original col that becomes position k
     * Q^T: result[perm_col[k]] = temp[k] */
    for (int k = 0; k < m; k++) {
        result[lu->perm_col[k]] = temp[k];
    }

    free(temp);
}

/**
 * @brief Forward transformation: solve Bx = b using LU + eta representation.
 *
 * Computes x = B^(-1) * column where B is the current basis matrix.
 * Uses LU factorization when available, followed by eta vector application.
 *
 * Algorithm:
 * 1. Copy input column to result
 * 2. If LU factors valid: apply LU solve (forward + backward substitution)
 * 3. Apply eta vectors in chronological order (oldest to newest)
 *
 * @param basis BasisState containing the factorization.
 * @param column Input column vector to transform (length = basis->m).
 * @param result Output array for transformed vector (length = basis->m).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_ftran(BasisState *basis, const double *column, double *result) {
    /* Validate arguments */
    if (basis == NULL || column == NULL || result == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = basis->m;

    /* Handle empty basis */
    if (m == 0) {
        return CXF_OK;
    }

    /* Step 1: Copy input column to result */
    memcpy(result, column, (size_t)m * sizeof(double));

    /* Step 2: Apply LU solve if factors are available */
    if (basis->lu != NULL && basis->lu->valid) {
        apply_lu_solve(basis->lu, m, result);
    } else if (basis->diag_coeff != NULL) {
        /* Fall back to diagonal scaling (legacy mode) */
        for (int i = 0; i < m; i++) {
            result[i] *= basis->diag_coeff[i];
        }
    }

    /* Step 3: Apply eta vectors in chronological order (oldest to newest) */
    int eta_count = basis->eta_count;
    if (eta_count == 0) {
        return CXF_OK;
    }

    /* Use stack allocation for small eta counts, heap for large */
    EtaFactors *stack_etas[MAX_STACK_ETAS];
    EtaFactors **etas = stack_etas;

    if (eta_count > MAX_STACK_ETAS) {
        etas = (EtaFactors **)malloc((size_t)eta_count * sizeof(EtaFactors *));
        if (etas == NULL) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    /* Traverse linked list to collect eta pointers
     * Head is newest, tail (last in list) is oldest */
    EtaFactors *eta = basis->eta_head;
    int count = 0;
    while (eta != NULL && count < eta_count) {
        etas[count++] = eta;
        eta = eta->next;
    }

    /* Apply eta vectors in chronological order (oldest to newest)
     * etas[0] = newest (head), etas[count-1] = oldest
     * So iterate from count-1 down to 0 */
    for (int i = count - 1; i >= 0; i--) {
        eta = etas[i];
        int pivot_row = eta->pivot_row;
        double pivot_elem = eta->pivot_elem;

        /* Bounds check pivot row */
        if (pivot_row < 0 || pivot_row >= m) {
            if (etas != stack_etas) {
                free(etas);
            }
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Numerical stability check */
        if (pivot_elem == 0.0 || !isfinite(pivot_elem)) {
            if (etas != stack_etas) {
                free(etas);
            }
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Apply eta transformation for E^(-1):
         *   factor = result[r] / pivot_elem
         *   result[r] = factor
         *   result[j] = result[j] - col[j] * factor  for j != r
         */
        double factor = result[pivot_row] / pivot_elem;
        result[pivot_row] = factor;

        /* Apply off-diagonal entries */
        for (int k = 0; k < eta->nnz; k++) {
            int j = eta->indices[k];
            if (j >= 0 && j < m && j != pivot_row) {
                result[j] -= eta->values[k] * factor;
            }
        }
    }

    /* Cleanup heap allocation if used */
    if (etas != stack_etas) {
        free(etas);
    }

    return CXF_OK;
}
