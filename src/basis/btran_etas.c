/**
 * @file btran_etas.c
 * @brief Shared basis helpers: eta collection, LU transpose solve, diagonal
 *        scaling, BTRAN eta application.
 *
 * Extracted from btran.c to keep every file under 200 LOC.
 * eta_collect / eta_validate / eta_collect_free are shared by FTRAN and BTRAN.
 * btran_apply_lu, btran_apply_diag, btran_apply_etas are BTRAN-specific.
 *
 * Spec: docs/specs/functions/basis/cxf_btran.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include "basis_internal.h"
#include <stdlib.h>
#include <math.h>

/*--- Shared eta collection helpers (used by FTRAN + BTRAN) ---------------*/

int eta_collect(BasisState *basis, EtaVector **stack_buf,
                EtaVector ***out_etas, int *out_count) {
    int eta_count = basis->eta_count;
    EtaVector **etas = stack_buf;

    if (eta_count > ETA_MAX_STACK) {
        etas = (EtaVector **)malloc((size_t)eta_count * sizeof(EtaVector *));
        if (etas == NULL) return CXF_ERROR_OUT_OF_MEMORY;
    }

    EtaVector *eta = basis->eta_head;
    int count = 0;
    while (eta != NULL && count < eta_count) {
        etas[count++] = eta;
        eta = eta->next;
    }

    *out_etas = etas;
    *out_count = count;
    return CXF_OK;
}

int eta_validate(const EtaVector *eta, int m) {
    if (eta->pivot_row < 0 || eta->pivot_row >= m)
        return CXF_ERROR_INVALID_ARGUMENT;
    if (eta->pivot_elem == 0.0 || !isfinite(eta->pivot_elem))
        return CXF_ERROR_INVALID_ARGUMENT;
    return CXF_OK;
}

void eta_collect_free(EtaVector **etas, EtaVector **stack_buf) {
    if (etas != stack_buf) free(etas);
}

/*--- BTRAN-specific helpers ---------------------------------------------*/

int btran_apply_lu(const LUFactors *lu, int m, double *result,
                   double *temp) {
    /* Step 1: Apply column permutation Q: temp = Q * result */
    for (int k = 0; k < m; k++) {
        temp[k] = result[lu->perm_col[k]];
    }

    /* Step 2: Solve U^T * z = temp (forward substitution) */
    for (int k = 0; k < m; k++) {
        if (fabs(lu->U_diag[k]) > CXF_MIN_PIVOT) {
            temp[k] /= lu->U_diag[k];
        }
        for (int64_t p = lu->U_col_ptr[k]; p < lu->U_col_ptr[k + 1]; p++) {
            int j = lu->U_row_idx[p];
            temp[j] -= lu->U_values[p] * temp[k];
        }
    }

    /* Step 3: Solve L^T * w = temp (backward substitution) */
    for (int k = m - 1; k >= 0; k--) {
        for (int64_t p = lu->L_col_ptr[k]; p < lu->L_col_ptr[k + 1]; p++) {
            int j = lu->L_row_idx[p];
            temp[k] -= lu->L_values[p] * temp[j];
        }
    }

    /* Step 4: Apply row permutation P^T: result = P^T * temp */
    for (int k = 0; k < m; k++) {
        result[lu->perm_row[k]] = temp[k];
    }
    return 0;
}

void btran_apply_diag(const double *diag_coeff, int m, double *result) {
    for (int i = 0; i < m; i++) {
        result[i] /= diag_coeff[i];
    }
}

int btran_apply_etas(BasisState *basis, int m, double *result) {
    if (basis->eta_count <= 0) return CXF_OK;

    EtaVector *stack_buf[ETA_MAX_STACK];
    EtaVector **etas;
    int count;
    int rc = eta_collect(basis, stack_buf, &etas, &count);
    if (rc != CXF_OK) return rc;

    /* Apply newest to oldest (etas[0] = newest) */
    for (int i = 0; i < count; i++) {
        EtaVector *eta = etas[i];
        /* Skip non-pivot etas (VARIABLE_FIX, BOUND_CHANGE, WARM_START) */
        if (eta->type != CXF_ETA_PIVOT) continue;
        rc = eta_validate(eta, m);
        if (rc != CXF_OK) { eta_collect_free(etas, stack_buf); return rc; }

        /* P2.5: Compute dot product; skip if both pivot and dot are zero */
        double dot = 0.0;
        for (int k = 0; k < eta->nnz; k++) {
            dot += eta->values[k] * result[eta->indices[k]];
        }
        if (result[eta->pivot_row] == 0.0 && dot == 0.0) continue;

        result[eta->pivot_row] =
            (result[eta->pivot_row] - dot) / eta->pivot_elem;
    }

    eta_collect_free(etas, stack_buf);
    return CXF_OK;
}
