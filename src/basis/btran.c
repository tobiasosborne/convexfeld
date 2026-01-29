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
 * @brief Backward transformation: solve y^T B = e_row^T.
 *
 * Computes y = B^(-T) * e_row where B is the current basis matrix.
 * The basis inverse is represented as a product of eta matrices:
 *   B^(-1) = E_k^(-1) * ... * E_2^(-1) * E_1^(-1)
 *
 * For BTRAN we need B^(-T) = (E_1^(-1))^T * ... * (E_k^(-1))^T
 * which requires applying eta transformations in REVERSE order
 * (newest to oldest).
 *
 * For each eta matrix E with pivot row r, pivot element p, and
 * off-diagonal values eta[j]:
 *   - Compute dot product: temp = sum(eta[j] * result[j]) for j != r
 *   - Update pivot: result[r] = (result[r] - temp) / p
 *   - Other positions unchanged
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

    /* Step 2: Collect eta pointers for reverse traversal */
    int eta_count = basis->eta_count;
    if (eta_count == 0) {
        /* Identity basis - result is already e_row */
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

    /* Traverse linked list to collect eta pointers */
    EtaFactors *eta = basis->eta_head;
    int count = 0;
    while (eta != NULL && count < eta_count) {
        etas[count++] = eta;
        eta = eta->next;
    }

    /* Step 3: Apply eta vectors in REVERSE order (newest to oldest)
     * etas[0] = newest (head), etas[count-1] = oldest
     * So iterate from 0 to count-1 */
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

        /* Compute dot product of off-diagonal entries with result:
         * temp = sum(eta->values[k] * result[eta->indices[k]])
         */
        double temp = 0.0;
        for (int k = 0; k < eta->nnz; k++) {
            int j = eta->indices[k];
            if (j >= 0 && j < m && j != pivot_row) {
                temp += eta->values[k] * result[j];
            }
        }

        /* Update pivot position:
         * result[r] = (result[r] - temp) / pivot_elem
         */
        result[pivot_row] = (result[pivot_row] - temp) / pivot_elem;

        /* Note: other positions unchanged in BTRAN */
    }

    /* Cleanup heap allocation if used */
    if (etas != stack_etas) {
        free(etas);
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

    /* Step 1: Initialize result = input (copy input to result) */
    memcpy(result, input, (size_t)m * sizeof(double));

    /* Step 2: Collect eta pointers for reverse traversal */
    int eta_count = basis->eta_count;
    if (eta_count == 0) {
        /* Identity basis - result is already input */
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

    /* Traverse linked list to collect eta pointers */
    EtaFactors *eta = basis->eta_head;
    int count = 0;
    while (eta != NULL && count < eta_count) {
        etas[count++] = eta;
        eta = eta->next;
    }

    /* Step 3: Apply eta vectors in REVERSE order (newest to oldest)
     * etas[0] = newest (head), etas[count-1] = oldest
     * So iterate from 0 to count-1 */
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

        /* Compute dot product of off-diagonal entries with result:
         * temp = sum(eta->values[k] * result[eta->indices[k]])
         */
        double temp = 0.0;
        for (int k = 0; k < eta->nnz; k++) {
            int j = eta->indices[k];
            if (j >= 0 && j < m && j != pivot_row) {
                temp += eta->values[k] * result[j];
            }
        }

        /* Update pivot position:
         * result[r] = (result[r] - temp) / pivot_elem
         */
        result[pivot_row] = (result[pivot_row] - temp) / pivot_elem;

        /* Note: other positions unchanged in BTRAN */
    }

    /* Cleanup heap allocation if used */
    if (etas != stack_etas) {
        free(etas);
    }

    return CXF_OK;
}
