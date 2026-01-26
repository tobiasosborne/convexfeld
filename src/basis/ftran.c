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
#include <string.h>

/**
 * @brief Forward transformation: solve Bx = b using eta representation.
 *
 * Computes x = B^(-1) * column where B is the current basis matrix.
 * The basis inverse is represented as a product of eta matrices:
 *   B^(-1) = E_k^(-1) * ... * E_2^(-1) * E_1^(-1)
 *
 * Algorithm:
 * 1. Copy input column to result (identity basis case)
 * 2. Apply each eta vector transformation in chronological order
 *    (oldest to newest) to compute the inverse product
 *
 * For each eta matrix E with pivot row r and pivot element p:
 *   - Save current value: temp = result[r]
 *   - Update pivot position: result[r] = temp / p
 *   - Update off-diagonal: result[j] -= eta_value[j] * temp
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

    /* Step 2: Apply eta vectors in chronological order (oldest to newest)
     * Traverse linked list from eta_head following next pointers */
    EtaFactors *eta = basis->eta_head;

    while (eta != NULL) {
        int pivot_row = eta->pivot_row;
        double pivot_elem = eta->pivot_elem;

        /* Bounds check pivot row */
        if (pivot_row < 0 || pivot_row >= m) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Division by zero check */
        if (pivot_elem == 0.0) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Save result at pivot position */
        double temp = result[pivot_row];

        /* Apply eta transformation:
         *   E^(-1) * x at pivot row: x[r] = x[r] / pivot_elem
         *   E^(-1) * x at other rows: x[j] -= eta[j] * temp
         */
        result[pivot_row] = temp / pivot_elem;

        /* Apply off-diagonal entries */
        for (int k = 0; k < eta->nnz; k++) {
            int j = eta->indices[k];
            if (j >= 0 && j < m && j != pivot_row) {
                result[j] -= eta->values[k] * temp;
            }
        }

        /* Move to next eta (newer) */
        eta = eta->next;
    }

    return CXF_OK;
}
