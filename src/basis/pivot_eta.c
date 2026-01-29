/**
 * @file pivot_eta.c
 * @brief Product Form of Inverse pivot update implementation.
 *
 * Implements basis update using eta vectors for the Product Form of
 * Inverse (PFI) representation. Creates Type 2 eta factors that represent
 * the basis change after a simplex pivot.
 *
 * Spec: docs/specs/functions/basis/cxf_pivot_with_eta.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

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
 * @return CXF_OK on success, CXF_ERROR_OUT_OF_MEMORY on allocation failure,
 *         -1 if pivot element is too small (|pivot| < CXF_PIVOT_TOL).
 */
int cxf_pivot_with_eta(BasisState *basis, int pivotRow, const double *pivotCol,
                       int enteringVar, int leavingVar) {
    /* Validate arguments */
    if (basis == NULL || pivotCol == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = basis->m;

    /* Validate pivot row index */
    if (pivotRow < 0 || pivotRow >= m) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Step 1: Validate pivot element magnitude */
    double pivot = pivotCol[pivotRow];
    if (fabs(pivot) < CXF_PIVOT_TOL) {
        return -1;  /* Pivot too small - caller should refactorize */
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

    /* Step 4: Allocate eta structure */
    EtaFactors *eta = (EtaFactors *)calloc(1, sizeof(EtaFactors));
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

    /* Allocate sparse arrays if needed */
    if (nnz > 0) {
        eta->indices = (int *)calloc((size_t)nnz, sizeof(int));
        eta->values = (double *)calloc((size_t)nnz, sizeof(double));

        if (eta->indices == NULL || eta->values == NULL) {
            free(eta->indices);
            free(eta->values);
            free(eta);
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
    basis->var_status[leavingVar] = -1;            /* Nonbasic at lower bound */
    basis->pivots_since_refactor++;

    return CXF_OK;
}
