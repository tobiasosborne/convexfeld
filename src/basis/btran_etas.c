/**
 * @file btran_etas.c
 * @brief Shared BTRAN helpers: eta traversal, LU transpose solve, diagonal
 *        scaling.
 *
 * Extracted from btran.c to keep every file under 200 LOC.
 * These helpers are called by both cxf_btran (unit-vector input) and
 * cxf_btran_vec (arbitrary-vector input).
 *
 * Spec: docs/specs/functions/basis/cxf_btran.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include "basis_internal.h"
#include <stdlib.h>
#include <math.h>

/** Maximum stack-allocated eta pointers before heap allocation */
#define MAX_STACK_ETAS 64

/**
 * @brief Apply LU transpose solve for BTRAN.
 *
 * Solves B^T * y = e using LU factors.
 * For B = P^T * L * U * Q:
 *   B^T = Q^T * U^T * L^T * P
 * So solve by: permute, U^T solve, L^T solve, permute back.
 *
 * @param lu LUFactors structure.
 * @param m Dimension.
 * @param result Vector (modified in place).
 * @param temp Scratch buffer of length m.
 */
int btran_apply_lu(const LUFactors *lu, int m, double *result,
                   double *temp) {

    /* Step 1: Apply column permutation Q: temp = Q * result
     * Q: temp[k] = result[perm_col[k]] */
    for (int k = 0; k < m; k++) {
        temp[k] = result[lu->perm_col[k]];
    }

    /* Step 2: Solve U^T * z = temp (forward substitution)
     * U^T is lower triangular (U is upper triangular)
     * For each row k, divide by diagonal then subtract from later rows */
    for (int k = 0; k < m; k++) {
        if (fabs(lu->U_diag[k]) > CXF_MIN_PIVOT) {
            temp[k] /= lu->U_diag[k];
        }
        /* U^T[j,k] = U[k,j] for j > k
         * U_col_ptr[k] to U_col_ptr[k+1] gives entries in U column k
         * above diagonal.  These become entries in U^T row k. */
        for (int64_t p = lu->U_col_ptr[k]; p < lu->U_col_ptr[k + 1]; p++) {
            int j = lu->U_row_idx[p];
            temp[j] -= lu->U_values[p] * temp[k];
        }
    }

    /* Step 3: Solve L^T * w = temp (backward substitution)
     * L^T is upper triangular with unit diagonal */
    for (int k = m - 1; k >= 0; k--) {
        for (int64_t p = lu->L_col_ptr[k]; p < lu->L_col_ptr[k + 1]; p++) {
            int j = lu->L_row_idx[p];
            temp[k] -= lu->L_values[p] * temp[j];
        }
    }

    /* Step 4: Apply row permutation P^T: result = P^T * temp
     * P^T: result[perm_row[k]] = temp[k] */
    for (int k = 0; k < m; k++) {
        result[lu->perm_row[k]] = temp[k];
    }

    return 0;
}

/**
 * @brief Apply diagonal scaling for BTRAN.
 *
 * For basis B = B_0 * E_1 * ... * E_k where B_0 = diag(coeff),
 * B^(-T) = B_0^(-T) * E_1^(-T) * ... * E_k^(-T).
 * This applies B_0^(-T) = diag(1/coeff).
 */
void btran_apply_diag(const double *diag_coeff, int m, double *result) {
    for (int i = 0; i < m; i++) {
        result[i] /= diag_coeff[i];
    }
}

/**
 * @brief Apply eta vectors in reverse order (newest to oldest) for BTRAN.
 *
 * Collects eta pointers from the linked list and applies each eta
 * inverse-transpose to result.  Uses stack allocation for small eta
 * counts, heap for large.
 *
 * @param basis BasisState containing the eta factorization.
 * @param m     Dimension.
 * @param result Vector modified in-place (length m).
 * @return CXF_OK on success, error code on failure.
 */
int btran_apply_etas(BasisState *basis, int m, double *result) {
    int eta_count = basis->eta_count;
    if (eta_count <= 0) {
        return CXF_OK;
    }

    /* Use stack allocation for small eta counts, heap for large */
    EtaVector *stack_etas[MAX_STACK_ETAS];
    EtaVector **etas = stack_etas;

    if (eta_count > MAX_STACK_ETAS) {
        etas = (EtaVector **)malloc((size_t)eta_count * sizeof(EtaVector *));
        if (etas == NULL) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    /* Traverse linked list to collect eta pointers */
    EtaVector *eta = basis->eta_head;
    int count = 0;
    while (eta != NULL && count < eta_count) {
        etas[count++] = eta;
        eta = eta->next;
    }

    /* Apply eta vectors: etas[0] = newest, iterate 0 to count-1 */
    for (int i = 0; i < count; i++) {
        eta = etas[i];
        int pivot_row = eta->pivot_row;
        double pivot_elem = eta->pivot_elem;

        /* Bounds check pivot row */
        if (pivot_row < 0 || pivot_row >= m) {
            if (etas != stack_etas) free(etas);
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Numerical stability check */
        if (pivot_elem == 0.0 || !isfinite(pivot_elem)) {
            if (etas != stack_etas) free(etas);
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* P2.5: Compute dot product; skip if both pivot and dot are zero */
        double dot = 0.0;
        for (int k = 0; k < eta->nnz; k++) {
            dot += eta->values[k] * result[eta->indices[k]];
        }
        if (result[pivot_row] == 0.0 && dot == 0.0) continue;

        /* Update pivot position */
        result[pivot_row] = (result[pivot_row] - dot) / pivot_elem;
    }

    /* Cleanup heap allocation if used */
    if (etas != stack_etas) {
        free(etas);
    }

    return CXF_OK;
}
