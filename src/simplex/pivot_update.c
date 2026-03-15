/**
 * @file pivot_update.c
 * @brief Incremental activity bound update after bound changes (F2 P3.19)
 *
 * Implements cancellation detection with conservative rounding and
 * infinity threshold transitions per numerical_stability.md Section B.
 *
 * Spec: docs/specs-v2/specs/modules/pivot_operations.md
 *       docs/specs-v2/specs/reference/numerical_stability.md Section B
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Conservative rounding factors (numerical_stability.md Section B) */
#define CANCEL_WIDEN_MIN  (1.0 + 1e-12)
#define CANCEL_WIDEN_MAX  (1.0 - 1e-12)

/**
 * Apply cancellation-safe incremental update to an activity bound.
 * When floating-point cancellation detected (addition not reversible),
 * apply conservative rounding to widen bounds.
 */
static double safe_add(double existing, double delta, int is_min) {
    double result = existing + delta;
    if ((result - delta) != existing) {
        result *= is_min ? CANCEL_WIDEN_MIN : CANCEL_WIDEN_MAX;
    }
    return result;
}

/**
 * Process one bound transition (finite-to-finite, finite-to-infinite,
 * or infinite-to-finite) for a single constraint row.
 */
static void apply_transition(double *act, int *unbd, double a,
                             double old_val, double new_val,
                             int old_inf, int new_inf, int is_min) {
    if (!old_inf && !new_inf) {
        /* Finite to finite: cancellation-safe delta */
        *act = safe_add(*act, a * (new_val - old_val), is_min);
    } else if (!old_inf && new_inf) {
        /* Finite to infinite: remove old finite contribution.
         * Use safe_add for cancellation detection (§B). */
        *act = safe_add(*act, -(a * old_val), is_min);
        if (unbd) (*unbd)++;
    } else if (old_inf && !new_inf) {
        /* Infinite to finite: add new finite contribution.
         * Use safe_add for cancellation detection (§B). */
        *act = safe_add(*act, a * new_val, is_min);
        if (unbd) (*unbd)--;
    }
    /* Infinite to infinite: no change */
}

/**
 * @brief Incrementally update constraint activity bounds when variable
 *        bounds change.
 *
 * Spec: pivot_operations.md cxf_pivot_update.
 * Replaces naive += with cancellation-detecting safe_add and handles
 * infinity threshold transitions with unbounded variable counts.
 *
 * @param state              Solver state with activity bounds and CSC matrix
 * @param col                Variable index whose bounds changed
 * @param oldLB              Previous lower bound value
 * @param newLB              Updated lower bound value
 * @param oldUB              Previous upper bound value
 * @param newUB              Updated upper bound value
 * @param infinityThreshold  Threshold beyond which a bound is infinite
 */
void cxf_pivot_update(SolverState *state, int col,
                      double oldLB, double newLB,
                      double oldUB, double newUB,
                      double infinityThreshold) {
    if (state == NULL) return;
    if (state->min_activity == NULL || state->max_activity == NULL) return;
    if (state->csc_col_ptr == NULL) return;
    if (col < 0 || col >= state->num_vars) return;

    int lb_changed = (fabs(newLB - oldLB) > 1e-15);
    int ub_changed = (fabs(newUB - oldUB) > 1e-15);
    if (!lb_changed && !ub_changed) return;

    double inf = infinityThreshold;
    int64_t start = state->csc_col_ptr[col];
    int64_t end = state->csc_col_ptr[col + 1];

    for (int64_t k = start; k < end; k++) {
        int row = state->csc_row_idx[k];
        double a = state->csc_values[k];

        if (lb_changed) {
            int oi = (oldLB <= -inf), ni = (newLB <= -inf);
            if (a > 0) {
                /* LB + positive coeff -> min_activity, negUnbdCount */
                apply_transition(&state->min_activity[row],
                    state->negUnbdCount ? &state->negUnbdCount[row] : NULL,
                    a, oldLB, newLB, oi, ni, 1);
            } else {
                /* LB + negative coeff -> max_activity, posUnbdCount */
                apply_transition(&state->max_activity[row],
                    state->posUnbdCount ? &state->posUnbdCount[row] : NULL,
                    a, oldLB, newLB, oi, ni, 0);
            }
        }

        if (ub_changed) {
            int oi = (oldUB >= inf), ni = (newUB >= inf);
            if (a > 0) {
                /* UB + positive coeff -> max_activity, posUnbdCount */
                apply_transition(&state->max_activity[row],
                    state->posUnbdCount ? &state->posUnbdCount[row] : NULL,
                    a, oldUB, newUB, oi, ni, 0);
            } else {
                /* UB + negative coeff -> min_activity, negUnbdCount */
                apply_transition(&state->min_activity[row],
                    state->negUnbdCount ? &state->negUnbdCount[row] : NULL,
                    a, oldUB, newUB, oi, ni, 1);
            }
        }
    }

    /* Update work counter proportional to column nonzeros */
    if (state->work_counter)
        *state->work_counter += (double)(end - start) * state->scale_factor;
}

/**
 * @brief Compute feasible step length bounds for ratio test (F2 P3.19).
 *
 * Given entering variable's pivot column and direction, compute the max
 * step before any basic variable hits its bound.
 *
 * Per harris_ratio_test.md Stage 1: let s = direction (+1 if entering
 * variable increases, -1 if decreases).  Basic variable i is driven
 * toward lb when s*d_i > 0, toward ub when s*d_i < 0.
 *
 * @param state      Solver state with bounds and basic vars
 * @param pivotCol   FTRAN result (dense, size m)
 * @param entering   Entering variable index
 * @param direction  Entering direction: +1 (increase) or -1 (decrease)
 * @param theta_max  Output: maximum feasible step
 * @return 0 on success, CXF_UNBOUNDED if no bound blocks
 */
int cxf_pivot_check(SolverState *state, const double *pivotCol,
                    int entering, int direction, double *theta_max) {
    if (state == NULL || pivotCol == NULL || theta_max == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = state->num_constrs;
    double best = CXF_INFINITY;
    int found = 0;

    for (int i = 0; i < m; i++) {
        double sd = direction * pivotCol[i];
        if (fabs(sd) < CXF_PIVOT_TOL) continue;

        int bv = state->basis->basic_vars[i];
        double x_bv = state->work_x[bv];
        double lb_bv = state->work_lb[bv];
        double ub_bv = state->work_ub[bv];
        double theta;

        if (sd > 0) {
            /* Driven toward lower bound */
            theta = (x_bv - lb_bv) / sd;
        } else {
            /* Driven toward upper bound */
            theta = (ub_bv - x_bv) / (-sd);
        }

        if (theta < 0) theta = 0;
        if (theta < best) {
            best = theta;
            found = 1;
        }
    }

    if (!found) return CXF_UNBOUNDED;

    *theta_max = best;
    return 0;
}
