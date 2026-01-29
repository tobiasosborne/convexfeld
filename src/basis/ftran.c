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
 * @brief Forward transformation: solve Bx = b using eta representation.
 *
 * Computes x = B^(-1) * column where B is the current basis matrix.
 * The basis inverse is represented as a product of eta matrices:
 *   B^(-1) = E_k^(-1) * ... * E_2^(-1) * E_1^(-1)
 *
 * Algorithm:
 * 1. Copy input column to result (identity basis case)
 * 2. Collect eta pointers for correct traversal order
 * 3. Apply each eta transformation in chronological order (oldest to newest)
 *
 * For each eta matrix E with pivot row r, pivot element p, and column values col[j]:
 *   - Compute factor = result[r] / p
 *   - Update pivot: result[r] = factor
 *   - Update off-diagonal: result[j] -= col[j] * factor
 *
 * @param basis BasisState containing the eta factorization.
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

    /* Step 1: Copy input column to result
     * This handles the identity basis case (no eta vectors) */
    memcpy(result, column, (size_t)m * sizeof(double));

    /* Step 2: Collect eta pointers for correct traversal order */
    int eta_count = basis->eta_count;
    if (eta_count == 0) {
        /* Identity basis - result is already column */
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

    /* Step 3: Apply eta vectors in chronological order (oldest to newest)
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
         *
         * Where eta->values stores col[j] (the pivot column values)
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
