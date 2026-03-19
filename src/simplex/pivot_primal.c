/**
 * @file pivot_primal.c
 * @brief Primal simplex pivot operation implementation.
 *
 * Implements cxf_pivot_primal as specified in:
 * docs/specs-v2/specs/modules/pivot_operations.md
 *
 * 7-step implementation:
 * 1. Bound feasibility check
 * 2. Pivot value determination from objective direction
 * 3. Objective value and coefficient update
 * 4. Variable status update (AT_LOWER/AT_UPPER)
 * 5. Working RHS propagation (CSC column scan)
 * 6. Eta vector creation for basis update chain
 * 7. Pricing notification (mark dirty + update var)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "../basis/basis_internal.h"
#include "simplex_internal.h"

/** @brief Threshold for determining if objective coefficient is significant */
#define TINY_THRESHOLD 1e-8

/**
 * @brief Execute primal simplex pivot operation.
 *
 * Pivots a non-basic variable to a new value based on objective coefficient
 * direction and bound positions. Updates objective value, variable status,
 * and constraint RHS values to maintain feasibility.
 *
 * Current implementation:
 * 1. Checks if bounds are tight enough to admit feasible pivot
 * 2. Determines appropriate pivot value based on objective direction
 * 3. Updates objective value and coefficient
 * 4. Marks variable status (AT_LOWER or AT_UPPER)
 * 5. Updates working RHS values: work_rhs[i] -= a_ij * pivotValue
 *
 * Deferred for future implementation:
 * - Create eta vector for basis representation (via cxf_pivot_with_eta)
 * - Handle piecewise linear and quadratic objectives
 * - Update neighbor relationships for combinatorial problems
 * - Update pricing state and steepest edge weights
 *
 * @param env Environment pointer (cast from void*)
 * @param state Solver context pointer (cast from void*)
 * @param var Index of variable to pivot
 * @param tolerance Numerical tolerance for feasibility checks
 * @return 0 on success, CXF_INFEASIBLE if bounds too tight, CXF_ERROR_OUT_OF_MEMORY on alloc failure
 */
int cxf_pivot_primal(void *env, void *state, int var, double tolerance) {
    CxfEnv *e;
    SolverState *ctx;
    double lb, ub, boundRange;
    double c, pivotValue;
    int n;

    /* Cast void pointers to proper types */
    e = (CxfEnv *)env;
    ctx = (SolverState *)state;

    /* Validate arguments */
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (e == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    n = ctx->num_vars;

    /* Validate variable index — structural [0,n) plus slacks [n,n+m) */
    if (var < 0 || var >= n + ctx->num_constrs) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate tolerance */
    if (tolerance <= 0.0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Step 1: Fixed-variable check
     *
     * Per numerical_stability.md Section C and tolerances_constants.md
     * Section 9: a variable whose bound range is within the bound
     * equality tolerance (~1e-10) is treated as fixed.  Fixed variables
     * are handled by the tight-bound routine, not declared infeasible.
     * Using the pricing tolerance here was wrong — it is orders of
     * magnitude too loose and would falsely reject variables with
     * small but legitimate bound ranges.
     */
    lb = ctx->work_lb[var];
    ub = ctx->work_ub[var];
    boundRange = ub - lb;

    if (2.0 * tolerance >= boundRange) {
        /* Variable is effectively fixed (binary: 2*caller_tol >= range) */
        return CXF_INFEASIBLE;
    }

    /* T2.14: Max-coefficient feasibility guard (binary Phase 2).
     * If max(|a_ij|) * bound_range > tolerance, fixing this variable
     * would cause excessive numerical disturbance — skip silently. */
    if (var < n && ctx->csc_col_ptr != NULL && ctx->csc_values != NULL) {
        int64_t cs = ctx->csc_col_ptr[var];
        int64_t ce = ctx->csc_col_ptr[var + 1];
        double max_coeff = 0.0;
        for (int64_t k = cs; k < ce; k++) {
            double av = fabs(ctx->csc_values[k]);
            if (av > max_coeff) max_coeff = av;
        }
        if (max_coeff * boundRange > tolerance)
            return 0;  /* CXF_OK — skip unsafe fix */
    }

    /*
     * Step 2: Pivot Value Determination
     *
     * Choose where to move the variable based on objective coefficient
     * and bound positions:
     *
     * - If objective coefficient is significant: move to favorable bound
     *   - For minimization (ConvexFeld default):
     *     - c <= 0: increase variable (move to upper bound)
     *     - c > 0:  decrease variable (move to lower bound)
     *
     * - If objective coefficient is tiny: structural decision
     *   - Both bounds same sign: use midpoint
     *   - Bounds straddle zero: pivot to zero
     */
    c = ctx->work_obj[var];

    if (fabs(c * boundRange) > TINY_THRESHOLD * tolerance) {
        /* Objective coefficient is significant - move to favorable bound */
        if (c <= 0.0) {
            /* Negative or zero coefficient: move to upper bound */
            pivotValue = ub;
        } else {
            /* Positive coefficient: move to lower bound */
            pivotValue = lb;
        }
    } else {
        /* Objective coefficient is tiny - use structural criterion */
        if (lb > 0.0 || ub < 0.0) {
            /* Both bounds same sign: use midpoint */
            pivotValue = 0.5 * (lb + ub);
        } else {
            /* Bounds straddle zero: pivot to zero */
            pivotValue = 0.0;
        }
    }

    /*
     * Step 3: Update Objective Value
     *
     * Add contribution from pivoted variable to objective:
     *   obj_value' = obj_value + c * pivotValue
     *
     * Then clear the variable's objective coefficient since it's
     * now effectively removed from the active optimization.
     */
    ctx->obj_value += c * pivotValue;
    ctx->work_obj[var] = 0.0;

    /*
     * Step 4: Update Variable Status
     *
     * Mark the variable as being at its chosen bound.
     * Use the same status codes as the basis module:
     *   -1 = AT_LOWER
     *   -2 = AT_UPPER
     *
     * For simplicity, determine status based on which bound
     * the pivot value is closer to.
     */
    if (ctx->basis != NULL && ctx->basis->var_status != NULL) {
        if (fabs(pivotValue - lb) < fabs(pivotValue - ub)) {
            /* Closer to lower bound */
            ctx->basis->var_status[var] = CXF_VAR_AT_LOWER;
        } else {
            /* Closer to upper bound */
            ctx->basis->var_status[var] = CXF_VAR_AT_UPPER;
        }
    }

    /*
     * Step 5: Working RHS Update
     *
     * After pivoting variable to new value, update working RHS of all
     * constraints that contain this variable. Uses SolverState working
     * copies (csc_col_ptr, csc_row_idx, csc_values, work_rhs) per V2
     * spec — never modifies the original model data.
     *
     * For each row i with coefficient a_ij:
     *   work_rhs[i] = work_rhs[i] - a_ij * pivotValue
     */
    /* Only structural vars (var < n) have CSC columns */
    if (var < n && ctx->csc_col_ptr != NULL && ctx->csc_row_idx != NULL &&
        ctx->csc_values != NULL) {

        int64_t col_start = ctx->csc_col_ptr[var];
        int64_t col_end = ctx->csc_col_ptr[var + 1];

        /* RHS update */
        if (ctx->work_rhs != NULL) {
            for (int64_t k = col_start; k < col_end; k++) {
                int row = ctx->csc_row_idx[k];
                double coeff = ctx->csc_values[k];
                if (row >= 0 && row < ctx->num_constrs)
                    ctx->work_rhs[row] -= coeff * pivotValue;
            }
        }

        /* T2.13: Activity update BEFORE CSC invalidation (reviewer fix) */
        cxf_pivot_update(ctx, var, lb, pivotValue, ub, pivotValue,
                         CXF_INFINITY);

        /* T2.13: Invalidate CSC entries AFTER activity update */
        for (int64_t k = col_start; k < col_end; k++)
            ctx->csc_row_idx[k] = -1;
    } else {
        /* Slack var or no CSC: activity update only */
        cxf_pivot_update(ctx, var, lb, pivotValue, ub, pivotValue,
                         CXF_INFINITY);
    }

    /*
     * Step 6: Eta vector creation (V2 pivot_operations.md)
     *
     * Record the variable elimination in the basis update chain.
     * Type CXF_ETA_VARIABLE_FIX documents the fixing operation.
     */
    if (ctx->basis != NULL) {
        EtaVector *eta;
        if (ctx->basis->eta_pool != NULL)
            eta = (EtaVector *)cxf_eta_pool_alloc(ctx->basis->eta_pool,
                                                    sizeof(EtaVector));
        else
            eta = (EtaVector *)calloc(1, sizeof(EtaVector));
        if (eta != NULL) {
            eta->type = CXF_ETA_VARIABLE_FIX;
            eta->pivot_row = -1;
            eta->entering_var = var;
            eta->leaving_var = -1;
            eta->pivot_elem = pivotValue;
            eta->reduced_cost = c;
            eta->direction = 0;
            eta->status = ctx->basis->var_status[var];
            eta->nnz = 0;
            eta->indices = NULL;
            eta->values = NULL;
            eta->next = ctx->basis->eta_head;
            ctx->basis->eta_head = eta;
            ctx->basis->eta_count++;
            ctx->eta_count = ctx->basis->eta_count;
        }
    }

    /*
     * Step 7: Pricing notification (V2 pricing_support.md)
     *
     * Mark the eliminated variable dirty so the pricing subsystem
     * refreshes reduced costs and candidate lists.
     */
    if (ctx->pricing != NULL) {
        cxf_pricing_update_var(ctx->pricing, ctx, var);
        cxf_pricing_mark_dirty(ctx->pricing, var);
    }

    return 0;  /* CXF_OK */
}
