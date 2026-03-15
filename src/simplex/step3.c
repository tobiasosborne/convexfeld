/**
 * @file step3.c
 * @brief Constraint-side bound propagation — cxf_simplex_step3 (v2 P3.20)
 *
 * For each dirty constraint, scan its CSR row and compute implied bounds
 * on each variable from the constraint's activity bounds. Tighten where
 * the implied bound is stronger than the current bound.
 *
 * This is the standard implied-bound technique from LP presolve
 * (Savelsbergh, 1994), applied iteratively during the simplex solve.
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
 * cxf_simplex_step3 — Constraint-side bound propagation (v2 P3.20, LP only)
 *
 * For each dirty constraint, scan its CSR row and compute implied bounds
 * on each variable from the constraint's activity bounds. Tighten where
 * the implied bound is stronger than the current bound.
 *
 * This is the standard implied-bound technique from LP presolve
 * (Savelsbergh, 1994), applied iteratively during the simplex solve.
 *---------------------------------------------------------------------------*/

int cxf_simplex_step3(SolverState *state, CxfEnv *env) {
    if (!state || !env) return 0;
    if (!state->pricing) return 0;
    if (!state->min_activity || !state->max_activity) return 0;

    if (!state->csr_row_ptr || !state->csr_col_idx || !state->csr_values)
        return 0;

    double tol = env->feasibility_tol;
    int m = state->num_constrs;
    int n = state->num_vars;
    int tightened = 0;

    /* Get dirty constraint candidates (V2 adaptive) */
    int num_cand = 0;
    int *candidates = NULL;
    cxf_pricing_constr_candidates_v2(state->pricing, state,
                                     &num_cand, &candidates);
    if (num_cand == 0 || candidates == NULL) return 0;

    for (int ci = 0; ci < num_cand; ci++) {
        int row = candidates[ci];
        if (row < 0 || row >= m) continue;

        double min_act = state->min_activity[row];
        double max_act = state->max_activity[row];
        if (min_act <= -CXF_INFINITY || max_act >= CXF_INFINITY) continue;

        char sense = (state->work_sense) ? state->work_sense[row] : '<';

        /* Two-stage infeasibility check (simplex_iteration.md).
         * Stage 1: preliminary — activity bounds indicate violation.
         * Stage 2: confirmation — recompute activity from scratch.
         * Prevents false infeasibility from accumulated numerical noise
         * in incrementally-maintained min/max activity. */
        {
            int stage1 = 0;
            if ((sense == '<' || sense == 'L') && min_act > tol)
                stage1 = 1;
            if ((sense == '>' || sense == 'G') && max_act < -tol)
                stage1 = 1;
            if ((sense == '=' || sense == 'E') &&
                (min_act > tol || max_act < -tol))
                stage1 = 1;
            if (stage1) {
                /* Stage 2: recompute activity from scratch */
                cxf_compute_activity_bounds(state, 1, &row);
                double fresh_min = state->min_activity[row];
                double fresh_max = state->max_activity[row];
                int confirmed = 0;
                if ((sense == '<' || sense == 'L') && fresh_min > tol)
                    confirmed = 1;
                if ((sense == '>' || sense == 'G') && fresh_max < -tol)
                    confirmed = 1;
                if ((sense == '=' || sense == 'E') &&
                    (fresh_min > tol || fresh_max < -tol))
                    confirmed = 1;
                if (confirmed)
                    return CXF_INFEASIBLE;
                /* Confirmation failed — continue processing */
                min_act = fresh_min;
                max_act = fresh_max;
            }
        }

        /* Scan CSR row for implied bounds */
        int64_t rs = state->csr_row_ptr[row];
        int64_t re = state->csr_row_ptr[row + 1];

        for (int64_t k = rs; k < re; k++) {
            int j = state->csr_col_idx[k];
            if (j < 0 || j >= n) continue;

            /* Skip basic or fixed variables */
            if (state->basis && state->basis->var_status[j] >= 0) continue;
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];
            if (ub - lb < tol) continue;

            double a = state->csr_values[k];
            if (fabs(a) < CXF_PIVOT_TOL) continue;

            /* Implied bounds from constraint activity (Savelsbergh 1994).
             * x_k <= l_k + (b_i - L_act_i) / a_ik  (for <= with a > 0)
             * x_k >= u_k + (b_i - U_act_i) / a_ik  (for <= with a < 0)
             * where L_act_i / U_act_i include variable k's contribution. */
            double rhs_i = (state->work_rhs) ? state->work_rhs[row] : 0.0;
            if (sense == '<' || sense == 'L' ||
                sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl = lb + (rhs_i - min_act) / a;
                    tightened += tighten_bound(state, j, impl, 0, tol);
                } else {
                    double impl = ub + (rhs_i - max_act) / a;
                    tightened += tighten_bound(state, j, impl, 1, tol);
                }
            }
            if (sense == '>' || sense == 'G' ||
                sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl = ub + (rhs_i - max_act) / a;
                    tightened += tighten_bound(state, j, impl, 1, tol);
                } else {
                    double impl = lb + (rhs_i - min_act) / a;
                    tightened += tighten_bound(state, j, impl, 0, tol);
                }
            }
        }
    }

    state->bounds_propagated += tightened;
    return 0;
}
