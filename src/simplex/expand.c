/**
 * @file expand.c
 * @brief EXPAND Mechanism B: implied bound analysis + bound widening.
 *
 * Extracted from perturbation.c for the 200 LOC limit.
 * Contains:
 *   - cxf_expand_analyze_basic(): implied bound analysis (v2 P2.6 Phase 4B)
 *   - cxf_expand_widen_bounds(): EXPAND bound widening (Mechanism B)
 *
 * Spec: docs/specs-v2/specs/algorithms/perturbation.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#include "simplex_internal.h"

#define MIN_BOUND_RANGE  1e-10
#define MAX_PERTURBATION 1e-6

static double clamp(double v, double lo, double hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/**
 * @brief Implied bound analysis for a basic variable (v2 P2.6 Phase 4B).
 *
 * Computes implied bounds from the constraint row using saved bounds
 * of other variables. Returns 1 if degenerate, -1 if infeasible, 0 ok.
 */
int cxf_expand_analyze_basic(SolverState *state, int row, int bvar,
                             double feas_tol) {
    if (!state->csr_row_ptr || !state->csr_col_idx || !state->csr_values)
        return 0;

    int n = state->num_vars;
    int64_t rs = state->csr_row_ptr[row];
    int64_t re = state->csr_row_ptr[row + 1];

    double impl_lo = 0.0, impl_hi = 0.0;
    int unbnd_lo = 0, unbnd_hi = 0;

    for (int64_t k = rs; k < re; k++) {
        int col = state->csr_col_idx[k];
        if (col < 0 || col == bvar) continue;
        double a = state->csr_values[k];

        /* Use saved (original) bounds to avoid perturbation drift */
        double s_lb, s_ub;
        if (col < n && state->saved_lb) {
            s_lb = state->saved_lb[col];
            s_ub = state->saved_ub[col];
        } else if (col < n && state->model_ref && state->model_ref->lb) {
            s_lb = state->model_ref->lb[col];
            s_ub = state->model_ref->ub[col];
        } else {
            s_lb = 0.0;
            s_ub = CXF_INFINITY;
        }

        if (a > 0.0) {
            if (s_ub < CXF_INFINITY) impl_lo += a * s_ub;
            else unbnd_lo++;
            if (s_lb > -CXF_INFINITY) impl_hi += a * s_lb;
            else unbnd_hi++;
        } else if (a < 0.0) {
            if (s_lb > -CXF_INFINITY) impl_lo += a * s_lb;
            else unbnd_lo++;
            if (s_ub < CXF_INFINITY) impl_hi += a * s_ub;
            else unbnd_hi++;
        }
    }

    double gap = clamp(impl_lo - impl_hi, MIN_BOUND_RANGE, MAX_PERTURBATION);
    char sense = (state->work_sense) ? state->work_sense[row] : '<';

    /* Equality constraint infeasibility */
    if ((sense == '=' || sense == 'E') &&
        impl_hi > gap * feas_tol && unbnd_hi == 0)
        return -1;

    /* Inequality degeneracy */
    if (impl_lo < -feas_tol * gap && unbnd_lo == 0)
        return 1;

    return 0;
}

/**
 * @brief EXPAND bound widening — Mechanism B (v2 perturbation.md Phase 5).
 *
 * Activates when stalling persists after Mechanism A (pricing restriction).
 * Primary: 100 consecutive degenerate pivots despite Mechanism A.
 * Fallback: iteration > 3*m with any recent degeneracy.
 *
 * @return Number of bounds widened (0 if not triggered).
 */
int cxf_expand_widen_bounds(SolverState *state, CxfEnv *env,
                            double feas_tol, BasisState *basis,
                            int m, int total) {
    /* Check activation conditions */
    int need_expand =
        !state->perturb_expand_active &&
        state->perturb_count > 0 &&
        state->degenerate_count > 100 &&
        state->saved_lb && state->saved_ub;
    if (!need_expand &&
        !state->perturb_expand_active &&
        state->perturb_count > 0 &&
        state->iteration > 3 * m &&
        state->degenerate_count > 0 &&
        state->saved_lb && state->saved_ub)
        need_expand = 1;

    if (!need_expand) return 0;

    int widened = 0;
    /* Spec: "epsilon_base is typically on the order of 1e-6 to 1e-8
     * (scaled from the feasibility tolerance)."
     * Use feas_tol/10 so default 1e-6 lands at 1e-7 (mid-range). */
    double eps_base = feas_tol / 10.0;
    if (eps_base < 1e-8) eps_base = 1e-8;
    if (eps_base > 1e-6) eps_base = 1e-6;

    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv < 0 || bv >= total) continue;
        double x = state->work_x[bv];
        double lb = state->work_lb[bv];
        double ub = state->work_ub[bv];

        /* Deterministic per-variable hash for distinct perturbations */
        unsigned h = (unsigned)bv * 2654435761u;
        double frac = (double)(h >> 1) / (double)(1u << 31);

        double eps = eps_base * (1.0 + fabs(lb > -CXF_INFINITY ? lb : 0.0))
                     * (1.0 + frac);

        if (fabs(x - lb) < feas_tol && lb > -CXF_INFINITY) {
            state->work_lb[bv] = state->saved_lb[bv] - eps;
            widened++;
        }
        if (fabs(x - ub) < feas_tol && ub < CXF_INFINITY) {
            double eps_u = eps_base
                * (1.0 + fabs(ub < CXF_INFINITY ? ub : 0.0))
                * (1.0 + frac);
            state->work_ub[bv] = state->saved_ub[bv] + eps_u;
            widened++;
        }
    }

    if (widened > 0) {
        state->perturb_expand_active = 1;

        /* Spec step 3: flag is set above.
         * Spec step 4: "Update constraint activities. Recompute
         * constraint activity bounds to reflect the modified working
         * bounds (via cxf_simplex_setup)." — perturbation.md */
        cxf_simplex_setup(state, env, 0, NULL);

        /* Recompute Phase I objective with widened bounds */
        if (state->phase == 1) {
            state->obj_value = 0.0;
            for (int i = 0; i < m; i++) {
                int bv = basis->basic_vars[i];
                if (bv < 0 || bv >= total) continue;
                double xv = state->work_x[bv];
                if (xv < state->work_lb[bv])
                    state->obj_value += state->work_lb[bv] - xv;
                else if (xv > state->work_ub[bv])
                    state->obj_value += xv - state->work_ub[bv];
            }
        }
    }

    return widened;
}
