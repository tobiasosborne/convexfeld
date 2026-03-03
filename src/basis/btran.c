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
 * Shared helpers (eta traversal, LU solve, diagonal scaling) live in
 * btran_etas.c to keep each file under 200 LOC.
 *
 * Spec: docs/specs/functions/basis/cxf_btran.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include "basis_internal.h"
#include <string.h>

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
    int rc = btran_apply_etas(basis, m, result);
    if (rc != CXF_OK) return rc;

    /* Step 3: Apply B_0^(-T) - must be done AFTER eta vectors */
    if (basis->lu != NULL && basis->lu->valid) {
        rc = btran_apply_lu(basis->lu, m, result, basis->work2);
        if (rc != 0) return rc;
    } else if (basis->diag_coeff != NULL) {
        btran_apply_diag(basis->diag_coeff, m, result);
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
 *   pi = B^(-T) * c_B
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
    int rc = btran_apply_etas(basis, m, result);
    if (rc != CXF_OK) return rc;

    /* Step 3: Apply B_0^(-T) - must be done AFTER eta vectors */
    if (basis->lu != NULL && basis->lu->valid) {
        rc = btran_apply_lu(basis->lu, m, result, basis->work2);
        if (rc != 0) return rc;
    } else if (basis->diag_coeff != NULL) {
        btran_apply_diag(basis->diag_coeff, m, result);
    }

    return CXF_OK;
}
