/**
 * @file step2.c
 * @brief Variable-side bound propagation — cxf_simplex_step2 (v2 P3.20)
 *
 * For each dirty variable, scan its CSC column to compute implied bounds
 * from all constraints it appears in. Tighten where the implication is
 * stronger than the current bound.
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#include "simplex_internal.h"


/**
 * @brief Tighten one variable bound and propagate.
 *
 * Updates the bound, adjusts activity bounds via cxf_pivot_update,
 * and marks the variable dirty in the pricing subsystem.
 *
 * @return 1 if tightened, 0 if no change
 */
static int tighten_bound(SolverState *state, int var, double new_val,
                         int is_lb, double tol) {
    double *bound = is_lb ? state->work_lb : state->work_ub;
    double old = bound[var];

    if (is_lb) {
        if (new_val <= old + tol) return 0;
    } else {
        if (new_val >= old - tol) return 0;
    }

    /* Check that tightening doesn't make bounds infeasible */
    if (is_lb && new_val > state->work_ub[var] + tol) return 0;
    if (!is_lb && new_val < state->work_lb[var] - tol) return 0;

    /* Capture old bounds before mutation */
    double old_lb = state->work_lb[var];
    double old_ub = state->work_ub[var];
    bound[var] = new_val;

    if (var < state->num_vars) {
        double new_lb = is_lb ? new_val : old_lb;
        double new_ub = is_lb ? old_ub : new_val;
        cxf_pivot_update(state, var, old_lb, new_lb, old_ub, new_ub,
                         CXF_INFINITY);
    }
    if (state->pricing)
        cxf_pricing_mark_dirty(state->pricing, var);
    return 1;
}

/*---------------------------------------------------------------------------
 * cxf_simplex_step2 — Variable-side bound propagation (v2 P3.20)
 *
 * For each dirty variable, scan its CSC column to compute implied bounds
 * from all constraints it appears in. Tighten where the implication is
 * stronger than the current bound.
 *---------------------------------------------------------------------------*/

int cxf_simplex_step2(SolverState *state, CxfEnv *env) {
    if (!state || !env) return 0;
    if (!state->pricing || !state->pricing->var_dirty) return 0;
    if (!state->min_activity || !state->max_activity) return 0;

    if (!state->csc_col_ptr) return 0;

    double tol = env->feasibility_tol;
    int n = state->num_vars;
    int tightened = 0;

    for (int j = 0; j < n; j++) {
        if (!state->pricing->var_dirty[j]) continue;

        /* Skip basic or already-fixed variables */
        if (state->basis && state->basis->var_status[j] >= 0) continue;
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];
        if (ub - lb < tol) continue;

        /* Two-stage infeasibility check (simplex_iteration.md).
         * Stage 1: preliminary — bounds crossed. Stage 2: confirmation —
         * recompute activity from scratch, restore bounds, re-derive.
         * Prevents false infeasibility from drifted activity bounds. */
        if (lb > ub + tol) {
            /* Stage 2: recompute activity for rows touching this var */
            int64_t cs2 = state->csc_col_ptr[j];
            int64_t ce2 = state->csc_col_ptr[j + 1];
            int rows2[64]; int nr2 = 0;
            for (int64_t k2 = cs2; k2 < ce2 && nr2 < 64; k2++)
                rows2[nr2++] = state->csc_row_idx[k2];
            if (nr2 > 0)
                cxf_compute_activity_bounds(state, nr2, rows2);

            /* Restore bounds from saved originals and re-derive */
            double safe_lb = state->saved_lb ? state->saved_lb[j] : lb;
            double safe_ub = state->saved_ub ? state->saved_ub[j] : ub;
            for (int64_t k2 = cs2; k2 < ce2; k2++) {
                int r2 = state->csc_row_idx[k2];
                double a2 = state->csc_values[k2];
                if (fabs(a2) < CXF_PIVOT_TOL) continue;
                double fmin = state->min_activity[r2];
                double fmax = state->max_activity[r2];
                if (fmin <= -CXF_INFINITY || fmax >= CXF_INFINITY) continue;
                double rhs2 = (state->work_rhs) ? state->work_rhs[r2] : 0.0;
                char s2 = state->work_sense ? state->work_sense[r2] : '<';
                if (s2 == '<' || s2 == 'L' || s2 == '=' || s2 == 'E') {
                    if (a2 > 0) {
                        double iu = safe_lb + (rhs2 - fmin) / a2;
                        if (iu < safe_ub) safe_ub = iu;
                    } else {
                        double il = safe_ub + (rhs2 - fmax) / a2;
                        if (il > safe_lb) safe_lb = il;
                    }
                }
                if (s2 == '>' || s2 == 'G' || s2 == '=' || s2 == 'E') {
                    if (a2 > 0) {
                        double il = safe_ub + (rhs2 - fmax) / a2;
                        if (il > safe_lb) safe_lb = il;
                    } else {
                        double iu = safe_lb + (rhs2 - fmin) / a2;
                        if (iu < safe_ub) safe_ub = iu;
                    }
                }
            }
            if (safe_lb > safe_ub + tol) return CXF_INFEASIBLE; /* Confirmed */
            /* Confirmation failed: restore bounds and continue */
            state->work_lb[j] = safe_lb;
            state->work_ub[j] = safe_ub;
            lb = safe_lb; ub = safe_ub;
            if (ub - lb < tol) continue;
        }

        /* Scan CSC column: for each constraint this variable appears in,
         * compute implied bound from constraint activity */
        int64_t cs = state->csc_col_ptr[j];
        int64_t ce = state->csc_col_ptr[j + 1];

        for (int64_t k = cs; k < ce; k++) {
            int row = state->csc_row_idx[k];
            double a = state->csc_values[k];
            if (fabs(a) < CXF_PIVOT_TOL) continue;

            double min_act = state->min_activity[row];
            double max_act = state->max_activity[row];
            if (min_act <= -CXF_INFINITY || max_act >= CXF_INFINITY) continue;

            char sense = (state->work_sense) ? state->work_sense[row] : '<';

            /* Implied bounds from constraint activity (Savelsbergh 1994).
             * x_k <= l_k + (b_i - L_act_i) / a_ik  (for <= with a > 0)
             * where L_act_i includes variable k's contribution. */
            double rhs_i = (state->work_rhs) ? state->work_rhs[row] : 0.0;
            if (sense == '<' || sense == 'L' || sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl_ub = lb + (rhs_i - min_act) / a;
                    tightened += tighten_bound(state, j, impl_ub, 0, tol);
                } else {
                    double impl_lb = ub + (rhs_i - max_act) / a;
                    tightened += tighten_bound(state, j, impl_lb, 1, tol);
                }
            }
            if (sense == '>' || sense == 'G' || sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl_lb = ub + (rhs_i - max_act) / a;
                    tightened += tighten_bound(state, j, impl_lb, 1, tol);
                } else {
                    double impl_ub = lb + (rhs_i - min_act) / a;
                    tightened += tighten_bound(state, j, impl_ub, 0, tol);
                }
            }

            /* Refresh bounds after possible tightening */
            lb = state->work_lb[j];
            ub = state->work_ub[j];
        }
    }

    /* Clear dirty flags */
    if (state->pricing->var_dirty && n > 0) {
        for (int j = 0; j < n; j++) state->pricing->var_dirty[j] = 0;
        state->pricing->num_dirty = 0;
    }

    state->flip_count += tightened;
    return 0;
}
