/**
 * @file step2.c
 * @brief BFRT deferred bound-flip processing — cxf_simplex_step2
 *
 * For each basic variable candidate, compute ratio = work_x[j] / pivot_coeff,
 * classify against bounds, and apply bound flips. Creates Type 7 etas.
 *
 * Binary algorithm (T1.1 source comparison):
 *   1. Get candidates from step2-specific pricing queue
 *   2. For each BASIC variable (var_status >= 0 = row index):
 *      - Find pivot coeff in CSR row
 *      - ratio = work_x[j] / pivot_coeff
 *   3. Classify: 0=none, 1=flip-upper, 2=flip-lower, 3=both
 *   4. Create Type 7 eta for each flip
 *   5. Call pivot_bound only for flip_type == 3
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

/* Type 7 eta: BFRT bound flip record */
#ifndef CXF_ETA_BOUND_FLIP
#define CXF_ETA_BOUND_FLIP 7
#endif

/**
 * @brief Find pivot coefficient for column j in CSR row.
 * @return coefficient value, or 0.0 if not found
 */
static double find_row_coeff(const SolverState *state, int row, int col) {
    int64_t rs = state->csr_row_ptr[row];
    int64_t re = state->csr_row_ptr[row + 1];
    for (int64_t k = rs; k < re; k++) {
        if (state->csr_col_idx[k] == col)
            return state->csr_values[k];
    }
    return 0.0;
}

/**
 * @brief Create a Type 7 (bound flip) eta record.
 * @return 0 on success, -1 on allocation failure
 */
static int create_flip_eta(SolverState *state, int var, int row,
                           double pivot_coeff, int flip_type) {
    BasisState *basis = state->basis;
    EtaVector *eta;
    if (basis->eta_pool)
        eta = (EtaVector *)cxf_eta_pool_alloc(basis->eta_pool,
                                               sizeof(EtaVector));
    else
        eta = (EtaVector *)calloc(1, sizeof(EtaVector));
    if (!eta) return -1;

    eta->type = CXF_ETA_BOUND_FLIP;
    eta->pivot_row = row;
    eta->entering_var = var;
    eta->leaving_var = -1;
    eta->pivot_elem = pivot_coeff;
    eta->reduced_cost = state->work_x[var];
    eta->direction = flip_type;
    eta->status = (flip_type == 1) ? CXF_VAR_AT_UPPER : CXF_VAR_AT_LOWER;
    eta->nnz = 0;
    eta->indices = NULL;
    eta->values = NULL;
    eta->next = basis->eta_head;
    basis->eta_head = eta;
    basis->eta_count++;
    state->eta_count = basis->eta_count;
    return 0;
}

/*---------------------------------------------------------------------------
 * cxf_simplex_step2 — BFRT deferred bound-flip processing
 *
 * Iterates basic variables, computes ratio, classifies flip type,
 * applies bound flips. Never returns CXF_INFEASIBLE.
 *---------------------------------------------------------------------------*/

int cxf_simplex_step2(SolverState *state, CxfEnv *env) {
    if (!state || !env) return 0;
    if (!state->basis || !state->basis->var_status) return 0;
    if (!state->csr_row_ptr || !state->csr_col_idx || !state->csr_values)
        return 0;

    BasisState *basis = state->basis;
    double tol = env->feasibility_tol;
    int m = state->num_constrs;
    int flips = 0;

    /* Process each basic variable (binary iterates pricing queue,
     * we iterate basic_vars[] for correctness — O(m)) */
    for (int row = 0; row < m; row++) {
        int j = basis->basic_vars[row];
        if (j < 0) continue;
        if (j >= state->num_vars + m) continue;

        /* Find pivot coefficient a_{row,j} in CSR row */
        double coeff = find_row_coeff(state, row, j);
        if (fabs(coeff) < CXF_MIN_PIVOT) continue;

        /* Compute ratio */
        double ratio = state->work_x[j] / coeff;

        /* Classify ratio against variable bounds */
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        int below = (ratio < lb - tol);
        int above = (ratio > ub + tol);
        int flip_type;

        if (below && above) flip_type = 3;
        else if (above)     flip_type = 1;
        else if (below)     flip_type = 2;
        else                continue; /* within bounds */

        /* Create Type 7 eta */
        create_flip_eta(state, j, row, coeff, flip_type);

        if (flip_type == 3) {
            /* Both bounds crossed: delegate to pivot_bound */
            double fix = (coeff > 0) ? ub : lb;
            cxf_pivot_bound(env, state, j, fix, ub, 0);
        } else if (flip_type == 1) {
            state->work_x[j] = ub;
        } else {
            state->work_x[j] = lb;
        }
        flips++;
    }

    state->flip_count += flips;
    return 0;
}
