/**
 * @file pivot_primal.c
 * @brief Primal simplex pivot operation implementation.
 *
 * Implements cxf_pivot_primal as specified in:
 * docs/specs/functions/pivot/cxf_pivot_primal.md
 *
 * This is a simplified implementation focusing on core pivot logic:
 * bound checking, value determination, and objective update. Full
 * matrix/eta support and constraint RHS updates are deferred until
 * constraint matrix access is available.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/** @brief Threshold for determining if objective coefficient is significant */
#define TINY_THRESHOLD 1e-8

/**
 * @brief Execute primal simplex pivot operation.
 *
 * Pivots a non-basic variable to a new value based on objective coefficient
 * direction and bound positions. Updates objective value and variable status.
 *
 * This is a simplified implementation that:
 * 1. Checks if bounds are tight enough to admit feasible pivot
 * 2. Determines appropriate pivot value based on objective direction
 * 3. Updates objective value and coefficient
 * 4. Marks variable status (AT_LOWER or AT_UPPER)
 *
 * Full implementation would also:
 * - Create eta vector for basis representation (via cxf_pivot_with_eta)
 * - Update constraint RHS values (requires sparse matrix access)
 * - Handle piecewise linear and quadratic objectives
 * - Update neighbor relationships for combinatorial problems
 *
 * @param env Environment pointer (cast from void*)
 * @param state Solver context pointer (cast from void*)
 * @param var Index of variable to pivot
 * @param tolerance Numerical tolerance for feasibility checks
 * @return 0 on success, 3 if infeasible, 1001 if out of memory
 */
int cxf_pivot_primal(void *env, void *state, int var, double tolerance) {
    CxfEnv *e;
    SolverContext *ctx;
    double lb, ub, boundRange;
    double c, pivotValue;
    int n;

    /* Cast void pointers to proper types */
    e = (CxfEnv *)env;
    ctx = (SolverContext *)state;

    /* Validate arguments */
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (e == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    n = ctx->num_vars;

    /* Validate variable index */
    if (var < 0 || var >= n) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate tolerance */
    if (tolerance <= 0.0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Step 1: Infeasibility Check
     *
     * If bounds are too tight (|ub - lb| < 2*tolerance), cannot determine
     * a feasible pivot value. Return infeasibility code.
     */
    lb = ctx->work_lb[var];
    ub = ctx->work_ub[var];
    boundRange = ub - lb;

    if (fabs(boundRange) < 2.0 * tolerance) {
        /* Bounds too tight - infeasible */
        return 3;  /* CXF_INFEASIBLE in primal pivot context */
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
            ctx->basis->var_status[var] = -1;  /* AT_LOWER */
        } else {
            /* Closer to upper bound */
            ctx->basis->var_status[var] = -2;  /* AT_UPPER */
        }
    }

    /*
     * TODO: Full implementation should include:
     *
     * 1. Constraint RHS updates:
     *    For each constraint i where a_ij != 0:
     *      rhs[i] -= a_ij * pivotValue
     *    (Requires sparse matrix access via model_ref)
     *
     * 2. Eta vector creation:
     *    Call cxf_pivot_with_eta to update basis representation
     *    (Deferred - cxf_pivot_with_eta handles standard basis exchange)
     *
     * 3. Piecewise linear objective handling:
     *    If variable has PWL flag, determine active segment and
     *    update objective slope accordingly
     *
     * 4. Quadratic objective handling:
     *    Update neighbor objective coefficients:
     *      obj[j] += Q[var,j] * pivotValue for all neighbors j
     *
     * 5. Pricing state update:
     *    Invalidate pricing cache and update steepest edge weights
     */

    return 0;  /* CXF_OK */
}
