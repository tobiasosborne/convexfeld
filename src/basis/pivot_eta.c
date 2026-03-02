/**
 * @file pivot_eta.c
 * @brief Product Form of Inverse pivot update implementation.
 *
 * P2.3: Uses arena allocator (EtaBuffer) when available for O(1) bulk
 * deallocation at refactorization. Falls back to individual calloc.
 *
 * Spec: docs/specs/functions/basis/cxf_pivot_with_eta.md
 * Beads: auj4 (P2.3)
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>  /* fabs, isfinite */

#include "basis_internal.h"

/* Arena allocator (eta_pool.c) */

/**
 * @brief Update basis using product form of inverse (eta vector).
 *
 * Creates an eta vector representing the basis change after a simplex pivot
 * and appends it to the eta list. The eta vector represents an elementary
 * transformation matrix that differs from the identity only in the pivot column.
 *
 * Algorithm:
 * 1. Validate pivot element is sufficiently large
 * 2. Compute eta multiplier = 1 / pivot
 * 3. Count nonzeros in pivot column (excluding pivot row)
 * 4. Allocate eta structure with sparse storage
 * 5. Store eta entries: eta[i] = -pivotCol[i] / pivot for i != pivotRow
 * 6. Link new eta to basis eta list (prepend to head)
 * 7. Update basis header and variable status arrays
 *
 * @param basis BasisState containing current basis factorization.
 * @param pivotRow Row index of leaving variable (0 <= pivotRow < m).
 * @param pivotCol Pivot column from FTRAN (B^(-1) * a_entering), length m.
 * @param enteringVar Index of entering variable.
 * @param leavingVar Index of leaving variable.
 * @param leavingStatus Nonbasic status for leaving var (CXF_VAR_AT_LOWER or
 *                      CXF_VAR_AT_UPPER). Caller determines from bound proximity.
 * @return CXF_OK on success, CXF_ERROR_OUT_OF_MEMORY on allocation failure,
 *         -1 if pivot element is too small (|pivot| < CXF_PIVOT_TOL).
 */
int cxf_pivot_with_eta(BasisState *basis, int pivotRow, const double *pivotCol,
                       int enteringVar, int leavingVar, int leavingStatus) {
    /* Validate arguments */
    if (basis == NULL || pivotCol == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = basis->m;

    /* Validate pivot row index */
    if (pivotRow < 0 || pivotRow >= m) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Step 1: Validate pivot element magnitude and finiteness */
    double pivot = pivotCol[pivotRow];
    if (!isfinite(pivot) || fabs(pivot) < CXF_PIVOT_TOL) {
        return -1;  /* Pivot non-finite or too small — caller should refactorize */
    }

    /* Step 2: Store pivot directly (not reciprocal) for correct FTRAN/BTRAN */

    /* Step 3: Count nonzeros in pivot column (excluding pivot row)
     * Drop values below CXF_ZERO_TOL to maintain sparsity */
    int nnz = 0;
    for (int i = 0; i < m; i++) {
        if (i != pivotRow && fabs(pivotCol[i]) > CXF_ZERO_TOL) {
            nnz++;
        }
    }

    /* Step 4: Allocate eta structure (P2.3: prefer pool, fallback calloc) */
    EtaVector *eta;
    if (basis->eta_pool != NULL) {
        eta = (EtaVector *)cxf_eta_pool_alloc(basis->eta_pool,
                                               sizeof(EtaVector));
    } else {
        eta = (EtaVector *)calloc(1, sizeof(EtaVector));
    }
    if (eta == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    eta->type = 2;              /* Type 2 = pivot update */
    eta->pivot_row = pivotRow;
    eta->pivot_var = enteringVar;
    eta->pivot_elem = pivot;    /* Store actual pivot, not reciprocal */
    eta->obj_coeff = 0.0;       /* Not used for pivot updates */
    eta->status = 0;            /* Not used for pivot updates */
    eta->nnz = nnz;
    eta->next = NULL;

    /* Allocate sparse arrays (P2.3: prefer pool) */
    if (nnz > 0) {
        if (basis->eta_pool != NULL) {
            eta->indices = (int *)cxf_eta_pool_alloc(basis->eta_pool,
                               (size_t)nnz * sizeof(int));
            eta->values = (double *)cxf_eta_pool_alloc(basis->eta_pool,
                               (size_t)nnz * sizeof(double));
        } else {
            eta->indices = (int *)calloc((size_t)nnz, sizeof(int));
            eta->values = (double *)calloc((size_t)nnz, sizeof(double));
        }

        if (eta->indices == NULL || eta->values == NULL) {
            /* Pool alloc failure — can't individually free pool memory */
            if (basis->eta_pool == NULL) {
                free(eta->indices);
                free(eta->values);
                free(eta);
            }
            return CXF_ERROR_OUT_OF_MEMORY;
        }

        /* Step 5: Store eta entries in sparse format
         * Store raw column values; FTRAN/BTRAN apply correct formulas */
        int k = 0;
        for (int i = 0; i < m; i++) {
            if (i != pivotRow && fabs(pivotCol[i]) > CXF_ZERO_TOL) {
                eta->indices[k] = i;
                eta->values[k] = pivotCol[i];  /* Store column value directly */
                k++;
            }
        }
    }

    /* Step 6: Link new eta to basis eta list (prepend to head)
     * New etas are added at the head for chronological ordering */
    eta->next = basis->eta_head;
    basis->eta_head = eta;
    basis->eta_count++;

    /* Step 7: Update basis state arrays */
    basis->basic_vars[pivotRow] = enteringVar;
    basis->var_status[enteringVar] = pivotRow;     /* Basic in this row */
    basis->var_status[leavingVar] = leavingStatus;
    basis->pivots_since_refactor++;

    return CXF_OK;
}
