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

extern void cxf_pivot_update(SolverState *state, int col,
                             double oldLB, double newLB,
                             double oldUB, double newUB,
                             double infinityThreshold);
extern void cxf_compute_activity_bounds(SolverState *state, int count,
                                        const int *indices);

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
                char s2 = state->work_sense ? state->work_sense[r2] : '<';
                if (s2 == '<' || s2 == 'L' || s2 == '=' || s2 == 'E') {
                    if (a2 > 0) {
                        double iu = safe_lb - fmin / a2;
                        if (iu < safe_ub) safe_ub = iu;
                    } else {
                        double il = safe_lb - fmax / a2;
                        if (il > safe_lb) safe_lb = il;
                    }
                }
                if (s2 == '>' || s2 == 'G' || s2 == '=' || s2 == 'E') {
                    if (a2 > 0) {
                        double il = safe_ub - fmax / a2;
                        if (il > safe_lb) safe_lb = il;
                    } else {
                        double iu = safe_ub - fmin / a2;
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
