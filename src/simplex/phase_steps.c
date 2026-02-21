/**
 * @file phase_steps.c
 * @brief Bidirectional bound propagation — step2 + step3 (v2 P3.20)
 *
 * cxf_simplex_step2: Variable-side — tighten bounds via CSC column scan
 * cxf_simplex_step3: Constraint-side — implied bounds (Savelsbergh 1994)
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

extern void cxf_pivot_update(SolverState *state, int var, double delta,
                             int is_lb);

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
    double delta = new_val - old;

    if (is_lb) {
        if (new_val <= old + tol) return 0;
    } else {
        if (new_val >= old - tol) return 0;
    }

    /* Check that tightening doesn't make bounds infeasible */
    if (is_lb && new_val > state->work_ub[var] + tol) return 0;
    if (!is_lb && new_val < state->work_lb[var] - tol) return 0;

    bound[var] = new_val;
    if (var < state->num_vars)
        cxf_pivot_update(state, var, delta, is_lb);
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

        /* Infeasibility check */
        if (lb > ub + tol) return CXF_INFEASIBLE;

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

            /* Implied bounds from constraint activity (Savelsbergh 1994):
             *
             * For <= constraint: sum a_ij x_j <= rhs
             *   a > 0: implied_ub = lb_j - min_activity[i] / a
             *   a < 0: implied_lb = lb_j - max_activity[i] / a
             *
             * For >= constraint: sum a_ij x_j >= rhs
             *   a > 0: implied_lb = ub_j - max_activity[i] / a
             *   a < 0: implied_ub = ub_j - min_activity[i] / a
             */
            if (sense == '<' || sense == 'L' || sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl_ub = lb - min_act / a;
                    tightened += tighten_bound(state, j, impl_ub, 0, tol);
                } else {
                    double impl_lb = lb - max_act / a;
                    tightened += tighten_bound(state, j, impl_lb, 1, tol);
                }
            }
            if (sense == '>' || sense == 'G' || sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl_lb = ub - max_act / a;
                    tightened += tighten_bound(state, j, impl_lb, 1, tol);
                } else {
                    double impl_ub = ub - min_act / a;
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

    /* Get dirty constraint candidates */
    int cand_buf[256];
    int *candidates = cand_buf;
    int num_cand = cxf_pricing_get_constr_candidates(
        state->pricing, candidates, 256);
    if (num_cand == 0) return 0;

    for (int ci = 0; ci < num_cand; ci++) {
        int row = candidates[ci];
        if (row < 0 || row >= m) continue;

        double min_act = state->min_activity[row];
        double max_act = state->max_activity[row];
        if (min_act <= -CXF_INFINITY || max_act >= CXF_INFINITY) continue;

        char sense = (state->work_sense) ? state->work_sense[row] : '<';

        /* Infeasibility check */
        if ((sense == '<' || sense == 'L') && min_act > tol)
            return CXF_INFEASIBLE;
        if ((sense == '>' || sense == 'G') && max_act < -tol)
            return CXF_INFEASIBLE;
        if (sense == '=' || sense == 'E') {
            if (min_act > tol || max_act < -tol)
                return CXF_INFEASIBLE;
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

            /* Implied bounds from constraint activity:
             *
             * For <= : a>0 → implied_ub = lb - min_act/a
             *          a<0 → implied_lb = lb - max_act/a
             * For >= : a>0 → implied_lb = ub - max_act/a
             *          a<0 → implied_ub = ub - min_act/a
             * For =  : both <= and >= rules apply
             */
            if (sense == '<' || sense == 'L' ||
                sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl = lb - min_act / a;
                    tightened += tighten_bound(state, j, impl, 0, tol);
                } else {
                    double impl = lb - max_act / a;
                    tightened += tighten_bound(state, j, impl, 1, tol);
                }
            }
            if (sense == '>' || sense == 'G' ||
                sense == '=' || sense == 'E') {
                if (a > 0) {
                    double impl = ub - max_act / a;
                    tightened += tighten_bound(state, j, impl, 1, tol);
                } else {
                    double impl = ub - min_act / a;
                    tightened += tighten_bound(state, j, impl, 0, tol);
                }
            }
        }
    }

    state->bounds_propagated += tightened;
    return 0;
}
