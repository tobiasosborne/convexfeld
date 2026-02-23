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

/* Arena allocator (eta_pool.c) */
extern void *cxf_eta_pool_alloc(EtaBuffer *pool, size_t size);

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

    /* Validate arguments */
    if (ctx == NULL || e == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (var < 0 || var >= ctx->num_vars) return CXF_ERROR_INVALID_ARGUMENT;

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

        eta->type = 3;                           /* Variable-fixing record */
        eta->pivot_row = -1;
        eta->pivot_var = var;
        eta->pivot_elem = new_value;
        eta->obj_coeff = ctx->work_obj[var];     /* Previous reduced cost */
        eta->status = basis->var_status[var];     /* Previous status */
        eta->nnz = 0;
        eta->indices = NULL;
        eta->values = NULL;
        eta->next = basis->eta_head;
        basis->eta_head = eta;
        basis->eta_count++;
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

    /* Phase 6: Activity bound propagation */
    old_lb = ctx->work_lb[var];
    old_ub = upperBound;
    if (ctx->min_activity != NULL && ctx->max_activity != NULL &&
        ctx->csc_col_ptr != NULL) {
        int64_t col_start = ctx->csc_col_ptr[var];
        int64_t col_end = ctx->csc_col_ptr[var + 1];
        int64_t k;
        for (k = col_start; k < col_end; k++) {
            int row = ctx->csc_row_idx[k];
            double coeff = ctx->csc_values[k];
            if (coeff > 0.0) {
                ctx->min_activity[row] += coeff * (new_value - old_lb);
                ctx->max_activity[row] += coeff * (new_value - old_ub);
            } else {
                ctx->min_activity[row] += coeff * (new_value - old_ub);
                ctx->max_activity[row] += coeff * (new_value - old_lb);
            }
        }
    }

    /* Phase 7: Matrix cleanup */
    ctx->work_lb[var] = new_value;
    ctx->work_ub[var] = new_value;
    if (basis != NULL && basis->var_status != NULL) {
        basis->var_status[var] = CXF_VAR_FIXED;
    }

    return CXF_OK;
}

/**
 * @brief Handle special pivot cases including unboundedness detection.
 *
 * Simplified implementation that:
 * 1. Determines if variable movement would improve objective
 * 2. Checks for unboundedness (infinite movement possible)
 * 3. Calls cxf_pivot_bound to move variable to appropriate bound if needed
 *
 * Full implementation would also:
 * - Scan constraint matrix to determine actual feasible movement
 * - Eliminate rows when variable can be fixed
 * - Check for special constraint flags (SOS, indicators, etc.)
 * - Update dual pricing arrays
 *
 * @param env Environment pointer (cast from void*)
 * @param state Solver context pointer (cast from void*)
 * @param var Variable index to analyze
 * @param lb_limit Lower bound limit for unbounded check (typically infinity)
 * @param ub_limit Upper bound limit for unbounded check (typically infinity)
 * @return CXF_OK (0) on success, CXF_UNBOUNDED (5) if unbounded,
 *         CXF_ERROR_OUT_OF_MEMORY (0x2711) on allocation failure
 */
int cxf_pivot_special(void *env, void *state, int var, double lb_limit,
                     double ub_limit) {
    CxfEnv *e;
    SolverState *ctx;
    double obj_coeff, lb, ub;
    int can_decrease, can_increase;
    int n;

    /* Cast void pointers to proper types */
    e = (CxfEnv *)env;
    ctx = (SolverState *)state;

    /* Validate arguments */
    if (ctx == NULL || e == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    n = ctx->num_vars;

    /* Validate variable index */
    if (var < 0 || var >= n) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Step 1: Extract objective coefficient and bounds */
    obj_coeff = ctx->work_obj[var];
    lb = ctx->work_lb[var];
    ub = ctx->work_ub[var];

    /* Step 2: Determine beneficial movement directions */
    /* For minimization:
     * - Positive objective: decreasing variable improves objective
     * - Negative objective: increasing variable improves objective
     */
    can_decrease = (obj_coeff > 1e-10 && lb > -CXF_INFINITY);
    can_increase = (obj_coeff < -1e-10 && ub < CXF_INFINITY);

    /* Step 3: If neither direction possible, return success */
    if (!can_decrease && !can_increase) {
        return CXF_OK;
    }

    /* Step 4: Check for unboundedness */
    if (can_increase) {
        /* Can improve by increasing - check if unbounded */
        if (ub >= ub_limit) {
            return CXF_UNBOUNDED;
        }
        /* Bounded - move to upper bound */
        return cxf_pivot_bound(env, state, var, ub, ub, 0);
    }

    if (can_decrease) {
        /* Can improve by decreasing - check if unbounded */
        if (lb <= -lb_limit) {
            return CXF_UNBOUNDED;
        }
        /* Bounded - move to lower bound */
        return cxf_pivot_bound(env, state, var, lb, ub, 0);
    }

    return CXF_OK;
}
