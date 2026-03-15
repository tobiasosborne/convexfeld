/**
 * @file pivot_special.c
 * @brief Special pivot case handling for LP solver.
 *
 * Implements cxf_pivot_bound and cxf_pivot_special as specified in:
 * - docs/specs-v2/specs/modules/pivot_operations.md
 *
 * cxf_pivot_bound: 7-phase variable fixing (eta, objective, pricing,
 *   activity propagation, matrix cleanup).
 * cxf_pivot_special: unboundedness detection and bound flipping.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"

/* Arena allocator (eta_pool.c) */

/* pivot_update.c — cancellation-safe activity bound update */

/**
 * @brief Fix a variable at a specific bound value (7-phase spec).
 *
 * Updates objective, creates eta record, notifies pricing, propagates
 * activity bounds, and marks the variable as fixed.
 *
 * @param env        Environment pointer (cast from void*)
 * @param state      Solver context pointer (cast from void*)
 * @param var        Variable index to fix
 * @param new_value  Value at which to fix the variable
 * @param upperBound Variable's current upper bound (for activity delta)
 * @param mode       0=unconditional, nonzero=check variable flags first
 * @return CXF_OK on success, CXF_ERROR_OUT_OF_MEMORY on eta alloc failure
 */
int cxf_pivot_bound(void *env, void *state, int var, double new_value,
                    double upperBound, int mode) {
    CxfEnv *e = (CxfEnv *)env;
    SolverState *ctx = (SolverState *)state;
    BasisState *basis;
    double old_lb, old_ub;

    /* Validate arguments — accept structural + slack variable indices */
    if (ctx == NULL || e == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (var < 0 || var >= ctx->num_vars + ctx->num_constrs)
        return CXF_ERROR_INVALID_ARGUMENT;

    basis = ctx->basis;

    /* Phase 1: Flag eval + eta record */
    /* mode != 0: check var flags (LP-only — no PWL/special flags to check) */
    (void)mode;
    if (basis != NULL) {
        EtaVector *eta;
        if (basis->eta_pool != NULL) {
            eta = (EtaVector *)cxf_eta_pool_alloc(basis->eta_pool,
                                                   sizeof(EtaVector));
        } else {
            eta = (EtaVector *)calloc(1, sizeof(EtaVector));
        }
        if (eta == NULL) return CXF_ERROR_OUT_OF_MEMORY;

        eta->type = CXF_ETA_VARIABLE_FIX;
        eta->pivot_row = -1;
        eta->entering_var = var;
        eta->leaving_var = -1;                   /* N/A for variable fixing */
        eta->pivot_elem = new_value;
        eta->reduced_cost = ctx->work_obj[var];  /* Previous reduced cost */
        eta->direction = 0;                      /* N/A for variable fixing */
        eta->status = basis->var_status[var];    /* Previous status */
        eta->nnz = 0;
        eta->indices = NULL;
        eta->values = NULL;
        eta->next = basis->eta_head;
        basis->eta_head = eta;
        basis->eta_count++;
        ctx->eta_count = basis->eta_count;  /* V2 sync: convexfeld-0ev4 */
    }

    /* Phase 2: Linear objective update */
    ctx->obj_value += ctx->work_obj[var] * new_value;
    ctx->work_obj[var] = 0.0;

    /* Phase 3: Quadratic — no Q-matrix in LP-only solver */
    /* Phase 4: Q-neighbor linearization — no Q-matrix in LP-only solver */

    /* Phase 5: Pricing notification */
    if (ctx->pricing != NULL) {
        cxf_pricing_update_var(ctx->pricing, ctx, var);
        cxf_pricing_mark_dirty(ctx->pricing, var);
    }

    /* Phase 6: Activity bound propagation (via cxf_pivot_update) */
    old_lb = ctx->work_lb[var];
    old_ub = upperBound;
    cxf_pivot_update(ctx, var, old_lb, new_value, old_ub, new_value,
                          CXF_INFINITY);

    /* Phase 7: Matrix cleanup */
    ctx->work_lb[var] = new_value;
    ctx->work_ub[var] = new_value;
    if (basis != NULL && basis->var_status != NULL) {
        basis->var_status[var] = CXF_VAR_FIXED;
    }

    return CXF_OK;
}

/**
 * @brief Handle special pivot cases: equality guard, unboundedness, bound flip.
 *
 * Five-phase algorithm per pivot_operations.md:
 *   Phase 1: Movement direction determination (reduced cost sign)
 *   Phase 2: Special constraint flag validation (SOS/indicator — LP deferred)
 *   Phase 3: Equality constraint column scan — if the variable participates
 *            in any equality constraint, return immediately (cannot eliminate)
 *   Phase 4: Action determination (bound flip vs unbounded vs row elimination)
 *   Phase 5: Execute action (bound flip, unbounded report, or row elimination)
 *
 * @param env       Environment pointer (cast from void*)
 * @param state     Solver context pointer (cast from void*)
 * @param var       Variable index to analyze
 * @param lb_limit  Lower bound limit for unbounded check (typically infinity)
 * @param ub_limit  Upper bound limit for unbounded check (typically infinity)
 * @return CXF_OK on success/no-op, CXF_UNBOUNDED if unbounded,
 *         CXF_ERROR_OUT_OF_MEMORY on allocation failure
 */
int cxf_pivot_special(void *env, void *state, int var, double lb_limit,
                     double ub_limit) {
    CxfEnv *e = (CxfEnv *)env;
    SolverState *ctx = (SolverState *)state;

    if (ctx == NULL || e == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    int n = ctx->num_vars;
    if (var < 0 || var >= n + ctx->num_constrs)
        return CXF_ERROR_INVALID_ARGUMENT;

    /* Phase 1: Movement direction determination */
    double obj_coeff = ctx->work_obj[var];
    double lb = ctx->work_lb[var];
    double ub = ctx->work_ub[var];

    int can_decrease = (obj_coeff > 1e-10 && lb > -CXF_INFINITY);
    int can_increase = (obj_coeff < -1e-10 && ub < CXF_INFINITY);

    if (!can_decrease && !can_increase)
        return CXF_OK;

    /* Phase 2: Special constraint flags — SOS/indicator (LP-only: no-op) */

    /* Phase 3: Equality constraint column scan (pivot_operations.md Phase 3).
     * If the variable appears in ANY equality constraint, it cannot be
     * eliminated by bound-fixing — the equality requires algebraic
     * substitution. Return CXF_OK to let normal simplex handle it.
     * Only structural variables (var < n) have CSC columns. */
    if (var < n && ctx->csc_col_ptr != NULL && ctx->work_sense != NULL) {
        int64_t col_start = ctx->csc_col_ptr[var];
        int64_t col_end   = ctx->csc_col_ptr[var + 1];
        for (int64_t k = col_start; k < col_end; k++) {
            int row = ctx->csc_row_idx[k];
            if (row >= 0 && row < ctx->num_constrs) {
                char sense = ctx->work_sense[row];
                if (sense == '=' || sense == 'E')
                    return CXF_OK;
            }
        }
    }

    /* Phase 4+5: Unboundedness check + bound flip.
     * Phase I suppression (pivot_operations.md line 247):
     * "A special mode flag on the solver state can disable unboundedness
     * detection; this is used during Phase I of two-phase simplex." */
    if (can_increase) {
        if (ub >= ub_limit) {
            if (ctx->phase == 1) return CXF_OK;
            return CXF_UNBOUNDED;
        }
        return cxf_pivot_bound(env, state, var, ub, ub, 0);
    }

    if (can_decrease) {
        if (lb <= -lb_limit) {
            if (ctx->phase == 1) return CXF_OK;
            return CXF_UNBOUNDED;
        }
        return cxf_pivot_bound(env, state, var, lb, ub, 0);
    }

    return CXF_OK;
}
