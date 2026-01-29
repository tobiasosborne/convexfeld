/**
 * @file lu_factors.c
 * @brief LUFactors structure lifecycle functions.
 *
 * Implements creation, destruction, and clearing of LU factorization storage.
 * The actual factorization algorithm is in refactor.c.
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create an LUFactors structure with preallocated storage.
 *
 * @param m Number of rows/columns in the basis.
 * @param L_nnz_estimate Estimated nonzeros in L (excluding diagonal).
 * @param U_nnz_estimate Estimated nonzeros in U (excluding diagonal).
 * @return Pointer to new LUFactors, or NULL on allocation failure.
 */
LUFactors *cxf_lu_create(int m, int64_t L_nnz_estimate, int64_t U_nnz_estimate) {
    if (m <= 0) {
        return NULL;
    }

    /* Ensure reasonable minimum estimates */
    if (L_nnz_estimate < m) L_nnz_estimate = m;
    if (U_nnz_estimate < m) U_nnz_estimate = m;

    LUFactors *lu = (LUFactors *)calloc(1, sizeof(LUFactors));
    if (lu == NULL) {
        return NULL;
    }

    lu->m = m;
    lu->valid = 0;
    lu->L_nnz = 0;
    lu->U_nnz = 0;

    /* Allocate L factor storage */
    lu->L_col_ptr = (int64_t *)calloc((size_t)(m + 1), sizeof(int64_t));
    lu->L_row_idx = (int *)malloc((size_t)L_nnz_estimate * sizeof(int));
    lu->L_values = (double *)malloc((size_t)L_nnz_estimate * sizeof(double));

    /* Allocate U factor storage */
    lu->U_col_ptr = (int64_t *)calloc((size_t)(m + 1), sizeof(int64_t));
    lu->U_row_idx = (int *)malloc((size_t)U_nnz_estimate * sizeof(int));
    lu->U_values = (double *)malloc((size_t)U_nnz_estimate * sizeof(double));
    lu->U_diag = (double *)malloc((size_t)m * sizeof(double));

    /* Allocate permutation arrays */
    lu->perm_row = (int *)malloc((size_t)m * sizeof(int));
    lu->perm_col = (int *)malloc((size_t)m * sizeof(int));

    /* Check all allocations succeeded */
    if (lu->L_col_ptr == NULL || lu->L_row_idx == NULL || lu->L_values == NULL ||
        lu->U_col_ptr == NULL || lu->U_row_idx == NULL || lu->U_values == NULL ||
        lu->U_diag == NULL || lu->perm_row == NULL || lu->perm_col == NULL) {
        cxf_lu_free(lu);
        return NULL;
    }

    /* Initialize permutations to identity */
    for (int i = 0; i < m; i++) {
        lu->perm_row[i] = i;
        lu->perm_col[i] = i;
    }

    return lu;
}

/**
 * @brief Free an LUFactors structure and all associated memory.
 *
 * @param lu LUFactors to free (may be NULL).
 */
void cxf_lu_free(LUFactors *lu) {
    if (lu == NULL) {
        return;
    }

    free(lu->L_col_ptr);
    free(lu->L_row_idx);
    free(lu->L_values);
    free(lu->U_col_ptr);
    free(lu->U_row_idx);
    free(lu->U_values);
    free(lu->U_diag);
    free(lu->perm_row);
    free(lu->perm_col);
    free(lu);
}

/**
 * @brief Clear LU factorization, marking it invalid.
 *
 * Resets the factorization without deallocating storage.
 *
 * @param lu LUFactors to clear.
 */
void cxf_lu_clear(LUFactors *lu) {
    if (lu == NULL) {
        return;
    }

    lu->valid = 0;
    lu->L_nnz = 0;
    lu->U_nnz = 0;

    /* Reset column pointers to empty columns */
    if (lu->L_col_ptr != NULL) {
        memset(lu->L_col_ptr, 0, (size_t)(lu->m + 1) * sizeof(int64_t));
    }
    if (lu->U_col_ptr != NULL) {
        memset(lu->U_col_ptr, 0, (size_t)(lu->m + 1) * sizeof(int64_t));
    }

    /* Reset permutations to identity */
    for (int i = 0; i < lu->m; i++) {
        if (lu->perm_row != NULL) lu->perm_row[i] = i;
        if (lu->perm_col != NULL) lu->perm_col[i] = i;
    }
}
