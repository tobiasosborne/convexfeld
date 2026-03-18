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
#include "basis_internal.h"
#include <string.h>
#include <math.h>

/**
 * @brief Apply LU forward/backward substitution.
 *
 * Solves B * x = b where B = P^T * L * U * Q (with permutations).
 * Steps: temp = P * b, L * w = temp, U * y = w, x = Q^T * y
 */
static int apply_lu_solve(const LUFactors *lu, int m, double *result,
                          double *temp) {
    /* Step 1: Permute input: temp = P * result */
    for (int k = 0; k < m; k++) {
        temp[k] = result[lu->perm_row[k]];
    }

    /* Step 2: Forward substitution L * w = temp */
    for (int k = 0; k < m; k++) {
        if (fabs(temp[k]) < CXF_MIN_PIVOT) continue;
        for (int64_t p = lu->L_col_ptr[k]; p < lu->L_col_ptr[k + 1]; p++) {
            int j = lu->L_row_idx[p];
            temp[j] -= lu->L_values[p] * temp[k];
        }
    }

    /* Step 3: Backward substitution U * y = temp */
    for (int k = m - 1; k >= 0; k--) {
        for (int64_t p = lu->U_col_ptr[k]; p < lu->U_col_ptr[k + 1]; p++) {
            int j = lu->U_row_idx[p];
            temp[k] -= lu->U_values[p] * temp[j];
        }
        if (fabs(lu->U_diag[k]) > CXF_MIN_PIVOT) {
            temp[k] /= lu->U_diag[k];
        }
    }

    /* Step 4: Permute output: result = Q^T * temp */
    for (int k = 0; k < m; k++) {
        result[lu->perm_col[k]] = temp[k];
    }
    return 0;
}

/**
 * @brief Apply eta vectors in chronological order (oldest to newest).
 *
 * Uses shared eta_collect / eta_validate / eta_collect_free helpers
 * from btran_etas.c.
 */
static int ftran_apply_etas(BasisState *basis, int m, double *result) {
    if (basis->eta_count <= 0) return CXF_OK;

    EtaVector *stack_buf[ETA_MAX_STACK];
    EtaVector **etas;
    int count;
    int rc = eta_collect(basis, stack_buf, &etas, &count);
    if (rc != CXF_OK) return rc;

    /* Oldest to newest: etas[0] = newest (head), iterate count-1 down to 0 */
    for (int i = count - 1; i >= 0; i--) {
        EtaVector *eta = etas[i];
        /* Skip non-pivot etas (VARIABLE_FIX, BOUND_CHANGE, WARM_START) —
         * these are metadata records, not basis transformations. */
        if (eta->type != CXF_ETA_PIVOT) continue;
        rc = eta_validate(eta, m);
        if (rc != CXF_OK) { eta_collect_free(etas, stack_buf); return rc; }

        /* P2.5: Hyper-sparse skip (Hall 2005) */
        if (result[eta->pivot_row] == 0.0) continue;

        /* E^(-1) application:
         *   factor = result[r] / pivot_elem
         *   result[r] = factor
         *   result[j] -= col[j] * factor  for j != r */
        double factor = result[eta->pivot_row] / eta->pivot_elem;
        result[eta->pivot_row] = factor;

        for (int k = 0; k < eta->nnz; k++) {
            result[eta->indices[k]] -= eta->values[k] * factor;
        }
    }

    eta_collect_free(etas, stack_buf);
    return CXF_OK;
}

/**
 * @brief Forward transformation: solve Bx = b using LU + eta.
 *
 * Computes x = B^(-1) * column where B is the current basis matrix.
 *
 * @param basis  BasisState containing the factorization.
 * @param column Input column vector (length = basis->m).
 * @param result Output vector (length = basis->m).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_ftran(BasisState *basis, const double *column, double *result) {
    if (basis == NULL || column == NULL || result == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    int m = basis->m;
    if (m == 0) return CXF_OK;

    /* Step 1: Copy input to result */
    memcpy(result, column, (size_t)m * sizeof(double));

    /* Step 2: Apply LU solve or diagonal fallback */
    if (basis->lu != NULL && basis->lu->valid) {
        int rc = apply_lu_solve(basis->lu, m, result, basis->work2);
        if (rc != 0) return rc;
    } else if (basis->diag_coeff != NULL) {
        for (int i = 0; i < m; i++) {
            result[i] /= basis->diag_coeff[i];
        }
    }

    /* Step 3: Apply eta vectors (oldest to newest) */
    return ftran_apply_etas(basis, m, result);
}
