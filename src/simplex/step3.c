/**
 * @file step3.c
 * @brief Constraint elimination — cxf_simplex_step3
 *
 * For each candidate constraint, pre-screen whether all nonbasic
 * variables have small range * coefficient products. If so, fix
 * every eligible nonbasic variable at the appropriate bound and
 * mark the row as eliminated (basic_vars = -2).
 *
 * Matches binary constraint-elimination algorithm (T1.2):
 *   1. Get constraint candidates from pricing queue
 *   2. Pre-screen: skip if ANY variable has |range*coeff| >= threshold
 *   3. Create Type 8 eta with full row data
 *   4. Fix each eligible nonbasic variable via cxf_pivot_bound
 *   5. Set basic_vars[row] = -2 (eliminated sentinel)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"

/* Type 8 eta: constraint elimination record with full row data */
#ifndef CXF_ETA_CONSTRAINT_ELIM
#define CXF_ETA_CONSTRAINT_ELIM 8
#endif

/**
 * @brief Pre-screen a constraint row for elimination eligibility.
 *
 * A constraint is eligible only if ALL nonbasic variables in its row
 * have |range * coeff| < threshold. Returns 1 if eligible, 0 if not.
 */
static int row_eligible(const SolverState *state, int row, double thresh) {
    int64_t rs = state->csr_row_ptr[row];
    int64_t re = state->csr_row_ptr[row + 1];
    int total = state->num_vars + state->num_constrs;

    for (int64_t k = rs; k < re; k++) {
        int j = state->csr_col_idx[k];
        if (j < 0 || j >= total) continue;
        if (state->basis->var_status[j] >= 0) continue; /* basic */

        double range = state->work_ub[j] - state->work_lb[j];
        if (range >= CXF_INFINITY) return 0;

        if (fabs(range * state->csr_values[k]) >= thresh) return 0;
    }
    return 1;
}

/**
 * @brief Create a Type 8 eta recording the eliminated row.
 *
 * Stores full row data (indices + values) for post-solve reversal.
 */
static int create_row_eta(SolverState *state, int row) {
    BasisState *basis = state->basis;
    int64_t rs = state->csr_row_ptr[row];
    int64_t re = state->csr_row_ptr[row + 1];
    int nnz = (int)(re - rs);
    if (nnz <= 0) return 0;

    EtaVector *eta;
    if (basis->eta_pool)
        eta = (EtaVector *)cxf_eta_pool_alloc(basis->eta_pool,
                                               sizeof(EtaVector));
    else
        eta = (EtaVector *)calloc(1, sizeof(EtaVector));
    if (!eta) return -1;

    int *idx = NULL;
    double *val = NULL;
    if (basis->eta_pool) {
        idx = (int *)cxf_eta_pool_alloc(basis->eta_pool,
                                         (size_t)nnz * sizeof(int));
        val = (double *)cxf_eta_pool_alloc(basis->eta_pool,
                                            (size_t)nnz * sizeof(double));
    } else {
        idx = (int *)malloc((size_t)nnz * sizeof(int));
        val = (double *)malloc((size_t)nnz * sizeof(double));
    }
    if (!idx || !val) {
        if (!basis->eta_pool) { free(idx); free(val); free(eta); }
        return -1;
    }

    for (int i = 0; i < nnz; i++) {
        idx[i] = state->csr_col_idx[rs + i];
        val[i] = state->csr_values[rs + i];
    }

    eta->type = CXF_ETA_CONSTRAINT_ELIM;
    eta->pivot_row = row;
    eta->entering_var = -1;
    eta->leaving_var = basis->basic_vars[row];
    eta->pivot_elem = 0.0;
    eta->reduced_cost = 0.0;
    eta->direction = 0;
    eta->status = 0;
    eta->nnz = nnz;
    eta->indices = idx;
    eta->values = val;
    eta->next = basis->eta_head;
    basis->eta_head = eta;
    basis->eta_count++;
    state->eta_count = basis->eta_count;
    return 0;
}

/*---------------------------------------------------------------------------
 * cxf_simplex_step3 — Constraint elimination
 *
 * For each dirty constraint candidate:
 *   1. Pre-screen: skip if any nonbasic var has |range*coeff| >= threshold
 *   2. Create Type 8 eta with full row data
 *   3. Fix all eligible nonbasic vars at appropriate bounds
 *   4. Mark row eliminated: basic_vars[row] = -2
 *---------------------------------------------------------------------------*/

int cxf_simplex_step3(SolverState *state, CxfEnv *env) {
    if (!state || !env) return 0;
    if (!state->pricing || !state->basis) return 0;
    if (!state->csr_row_ptr || !state->csr_col_idx || !state->csr_values)
        return 0;

    double thresh = env->feasibility_tol;
    int m = state->num_constrs;
    int total = state->num_vars + m;

    int num_cand = 0;
    int *candidates = NULL;
    cxf_pricing_constr_candidates_v2(state->pricing, state,
                                     &num_cand, &candidates);
    if (num_cand == 0 || !candidates) return 0;

    for (int ci = 0; ci < num_cand; ci++) {
        int row = candidates[ci];
        if (row < 0 || row >= m) continue;
        if (state->basis->basic_vars[row] == -2) continue; /* eliminated */

        char sense = (state->work_sense) ? state->work_sense[row] : '<';
        if (sense == '=' || sense == 'E') continue; /* can't eliminate */

        if (!row_eligible(state, row, thresh)) continue;

        if (create_row_eta(state, row) < 0) continue;

        /* Fix all eligible nonbasic vars at appropriate bounds.
         * <= constraint: positive coeff → lb, negative → ub.
         * >= constraint: positive coeff → ub, negative → lb. */
        int is_ge = (sense == '>' || sense == 'G');
        int64_t rs = state->csr_row_ptr[row];
        int64_t re = state->csr_row_ptr[row + 1];

        for (int64_t k = rs; k < re; k++) {
            int j = state->csr_col_idx[k];
            if (j < 0 || j >= total) continue;
            if (state->basis->var_status[j] >= 0) continue;

            double a = state->csr_values[k];
            double ub = state->work_ub[j];
            double fix = is_ge ? ((a > 0.0) ? ub : state->work_lb[j])
                               : ((a > 0.0) ? state->work_lb[j] : ub);

            cxf_pivot_bound(env, state, j, fix, ub, 0);
            state->bounds_propagated++;
        }

        state->basis->basic_vars[row] = -2;
        state->rows_eliminated++;
    }

    return 0;
}
