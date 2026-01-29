/**
 * @file btran.c
 * @brief Backward transformation (BTRAN) implementation (M5.1.5)
 *
 * Implements backward transformation to compute y = B^(-T) * e_row,
 * where B is the current basis matrix represented using Product Form
 * of Inverse (PFI) with eta vectors.
 *
 * BTRAN solves y^T B = e_row^T, used for computing simplex tableau rows.
 *
 * Spec: docs/specs/functions/basis/cxf_btran.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
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
 */
static void apply_lu_btran(const LUFactors *lu, int m, double *result) {
    double *temp = (double *)malloc((size_t)m * sizeof(double));
    if (temp == NULL) return;

    /* Step 1: Apply column permutation Q: temp = Q * result
     * Q: temp[k] = result[perm_col[k]] */
    for (int k = 0; k < m; k++) {
        temp[k] = result[lu->perm_col[k]];
    }

    /* Step 2: Solve U^T * z = temp (forward substitution)
     * U^T is lower triangular (U is upper triangular)
     * For each row k, divide by diagonal then subtract from later rows */
    for (int k = 0; k < m; k++) {
        if (fabs(lu->U_diag[k]) > 1e-15) {
            temp[k] /= lu->U_diag[k];
        }
        /* U^T[j,k] = U[k,j] for j > k
         * U_col_ptr[k] to U_col_ptr[k+1] gives entries in U column k above diagonal
         * These become entries in U^T row k to the right */
        for (int64_t p = lu->U_col_ptr[k]; p < lu->U_col_ptr[k + 1]; p++) {
            int j = lu->U_row_idx[p];  /* j > k (step index in upper triangle) */
            temp[j] -= lu->U_values[p] * temp[k];
        }
    }

    /* Step 3: Solve L^T * w = temp (backward substitution)
     * L^T is upper triangular with unit diagonal (L is lower with unit diag)
     * For each column k from m-1 to 0, subtract from result[k] */
    for (int k = m - 1; k >= 0; k--) {
        /* L^T[k,j] = L[j,k] for j > k
         * L_col_ptr[k] gives entries in L column k below diagonal */
        for (int64_t p = lu->L_col_ptr[k]; p < lu->L_col_ptr[k + 1]; p++) {
            int j = lu->L_row_idx[p];  /* j > k (row index in lower triangle) */
            temp[k] -= lu->L_values[p] * temp[j];
        }
        /* Unit diagonal, no division needed */
    }

    /* Step 4: Apply row permutation P^T: result = P^T * temp
     * P: perm_row[k] = original row at position k
     * P^T: result[perm_row[k]] = temp[k] */
    for (int k = 0; k < m; k++) {
        result[lu->perm_row[k]] = temp[k];
    }

    free(temp);
}

/**
 * @brief Apply diagonal scaling for BTRAN.
 *
 * For basis B = B_0 * E_1 * ... * E_k where B_0 = diag(coeff),
 * B^(-T) = B_0^(-T) * E_1^(-T) * ... * E_k^(-T)
 *
 * This applies B_0^(-T) = diag(1/coeff).
 * Since diag_coeff is ±1, 1/coeff = coeff.
 */
static void apply_diag_btran(const double *diag_coeff, int m, double *result) {
    for (int i = 0; i < m; i++) {
        result[i] *= diag_coeff[i];
    }
}

/**
 * @brief Backward transformation: solve y^T B = e_row^T.
 *
 * Computes y = B^(-T) * e_row where B is the current basis matrix.
 * The basis is represented as B = B_0 * E_1 * ... * E_k where:
 *   - B_0 is the initial diagonal basis (diag_coeff)
 *   - E_i are eta matrices from pivots
 *
 * So B^(-T) = B_0^(-T) * E_1^(-T) * ... * E_k^(-T)
 *
 * To compute B^(-T) * y:
 * 1. Apply E_k^(-T), E_{k-1}^(-T), ..., E_1^(-T) (newest to oldest)
 * 2. Apply B_0^(-T) last
 *
 * @param basis BasisState containing the eta factorization.
 * @param row Row index for unit vector e_row (0 <= row < basis->m).
 * @param result Output array for transformed vector (length = basis->m).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_btran(BasisState *basis, int row, double *result) {
    /* Validate arguments */
    if (basis == NULL || result == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (row < 0 || row >= basis->m) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    int m = basis->m;

    /* Handle empty basis */
    if (m == 0) {
        return CXF_OK;
    }

    /* Step 1: Initialize result as unit vector e_row */
    memset(result, 0, (size_t)m * sizeof(double));
    result[row] = 1.0;

    /* Step 2: Apply eta vectors in reverse order (newest to oldest) */
    int eta_count = basis->eta_count;
    if (eta_count > 0) {
        /* Use stack allocation for small eta counts, heap for large */
        EtaFactors *stack_etas[MAX_STACK_ETAS];
        EtaFactors **etas = stack_etas;

        if (eta_count > MAX_STACK_ETAS) {
            etas = (EtaFactors **)malloc((size_t)eta_count * sizeof(EtaFactors *));
            if (etas == NULL) {
                return CXF_ERROR_OUT_OF_MEMORY;
            }
        }

        /* Traverse linked list to collect eta pointers */
        EtaFactors *eta = basis->eta_head;
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

            /* Compute dot product of off-diagonal entries with result */
            double temp = 0.0;
            for (int k = 0; k < eta->nnz; k++) {
                int j = eta->indices[k];
                if (j >= 0 && j < m && j != pivot_row) {
                    temp += eta->values[k] * result[j];
                }
            }

            /* Update pivot position */
            result[pivot_row] = (result[pivot_row] - temp) / pivot_elem;
        }

        /* Cleanup heap allocation if used */
        if (etas != stack_etas) {
            free(etas);
        }
    }

    /* Step 3: Apply B_0^(-T) - must be done AFTER eta vectors */
    if (basis->lu != NULL && basis->lu->valid) {
        apply_lu_btran(basis->lu, m, result);
    } else if (basis->diag_coeff != NULL) {
        apply_diag_btran(basis->diag_coeff, m, result);
    }

    return CXF_OK;
}

/**
 * @brief Backward transformation with arbitrary input vector.
 *
 * Computes y = B^(-T) * input where B is the current basis matrix.
 * Unlike cxf_btran which takes a row index for unit vector input,
 * this function accepts any input vector.
 *
 * This is needed for computing dual prices (simplex multipliers):
 *   π = B^(-T) * c_B
 * where c_B is the objective coefficients of basic variables.
 *
 * @param basis BasisState containing the eta factorization.
 * @param input Input vector (length = basis->m).
 * @param result Output array for transformed vector (length = basis->m).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_btran_vec(BasisState *basis, const double *input, double *result) {
    /* Validate arguments */
    if (basis == NULL || input == NULL || result == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = basis->m;

    /* Handle empty basis */
    if (m == 0) {
        return CXF_OK;
    }

    /* Step 1: Initialize result = input */
    memcpy(result, input, (size_t)m * sizeof(double));

    /* Step 2: Apply eta vectors in reverse order (newest to oldest) */
    int eta_count = basis->eta_count;
    if (eta_count > 0) {
        /* Use stack allocation for small eta counts, heap for large */
        EtaFactors *stack_etas[MAX_STACK_ETAS];
        EtaFactors **etas = stack_etas;

        if (eta_count > MAX_STACK_ETAS) {
            etas = (EtaFactors **)malloc((size_t)eta_count * sizeof(EtaFactors *));
            if (etas == NULL) {
                return CXF_ERROR_OUT_OF_MEMORY;
            }
        }

        /* Traverse linked list to collect eta pointers */
        EtaFactors *eta = basis->eta_head;
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

            /* Compute dot product of off-diagonal entries with result */
            double temp = 0.0;
            for (int k = 0; k < eta->nnz; k++) {
                int j = eta->indices[k];
                if (j >= 0 && j < m && j != pivot_row) {
                    temp += eta->values[k] * result[j];
                }
            }

            /* Update pivot position */
            result[pivot_row] = (result[pivot_row] - temp) / pivot_elem;
        }

        /* Cleanup heap allocation if used */
        if (etas != stack_etas) {
            free(etas);
        }
    }

    /* Step 3: Apply B_0^(-T) - must be done AFTER eta vectors */
    if (basis->lu != NULL && basis->lu->valid) {
        apply_lu_btran(basis->lu, m, result);
    } else if (basis->diag_coeff != NULL) {
        apply_diag_btran(basis->diag_coeff, m, result);
    }

    return CXF_OK;
}
