/**
 * @file perturbation.c
 * @brief EXPAND anti-cycling perturbation (v2 P2.6, P3.21)
 *
 * Implied bound analysis removes irrecoverably degenerate variables
 * from the pricing set. Uses saved (original) bounds to avoid
 * perturbation drift. Degenerate variables set to AT_UPPER (not
 * FIXED) so they remain eligible as ratio test blockers.
 *
 * Spec: docs/specs-v2/specs/algorithms/perturbation.md
 *       docs/specs-v2/specs/modules/simplex_phases.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <string.h>

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
static int analyze_basic(SolverState *state, const MatrixData *mat,
                         int row, int bvar, double feas_tol) {
    if (!mat->row_ptr || !mat->col_idx || !mat->row_values) return 0;

    int n = state->num_vars;
    int64_t rs = mat->row_ptr[row];
    int64_t re = mat->row_ptr[row + 1];

    double impl_lo = 0.0, impl_hi = 0.0;
    int unbnd_lo = 0, unbnd_hi = 0;

    for (int64_t k = rs; k < re; k++) {
        int col = mat->col_idx[k];
        if (col < 0 || col == bvar) continue;
        double a = mat->row_values[k];

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
    char sense = (mat->sense) ? mat->sense[row] : '<';

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
 * @brief Apply EXPAND anti-cycling perturbation (v2 P2.6).
 *
 * Phase 1: Save current bounds if not already saved
 * Phase 2: Process nonbasic variables — remove degenerate from pricing
 * Phase 3: Process basic variables — implied bound analysis
 * Phase 4: Notify pricing of changes
 */
int cxf_simplex_perturbation(SolverState *state, CxfEnv *env) {
    if (!state || !env) return CXF_ERROR_NULL_ARGUMENT;

    int n = state->num_vars;
    int m = state->num_constrs;
    if (n == 0 || m == 0) return CXF_OK;
    /* P1.6 (zr5l): removed iteration==0 guard — proactive perturbation
     * is now called in early iterations by solve_lp.c */

    BasisState *basis = state->basis;
    if (!basis || !basis->var_status) return CXF_OK;

    CxfModel *model = state->model_ref;
    if (!model) return CXF_ERROR_NULL_ARGUMENT;

    double feas_tol = env->feasibility_tol;
    if (feas_tol <= 0.0) feas_tol = CXF_FEASIBILITY_TOL;

    int total = n + m;
    int perturbed = 0;

    /*--- Phase 1: Save bounds for later restoration ---*/
    if (state->perturb_count == 0 && state->saved_lb && state->saved_ub) {
        memcpy(state->saved_lb, state->work_lb,
               (size_t)total * sizeof(double));
        memcpy(state->saved_ub, state->work_ub,
               (size_t)total * sizeof(double));
    }

    /*--- Phase 2: Nonbasic variables at bounds ---
     * P1.7 (a5vp): candidate removal, NOT bound modification.
     * Spec P2.6: "removes degenerate candidates from pricing set...
     * avoids modifying bound arrays." We mark degenerate variables
     * dirty in pricing to exclude them, without touching work_x. */
    double *dj = state->work_dj;
    if (dj) {
        for (int j = 0; j < total; j++) {
            if (basis->var_status[j] != CXF_VAR_AT_LOWER) continue;

            double rc = dj[j];
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];

            /* Skip if already fixed */
            if (ub - lb < feas_tol) continue;

            if (fabs(rc) <= feas_tol) {
                /* Near-zero RC at lower bound: degenerate candidate.
                 * Mark dirty to remove from pricing set. */
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            } else if (rc < -feas_tol) {
                /* Negative RC at lower bound: check equality constraints */
                if (j >= n && (j - n) < m && model->matrix &&
                    model->matrix->sense) {
                    char sense = model->matrix->sense[j - n];
                    if (sense == '=' || sense == 'E') {
                        state->problem_row_index = j - n;
                        state->perturb_count += perturbed;
                        return CXF_INFEASIBLE;
                    }
                }
                /* For inequalities: mark dirty to remove from pricing */
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            }
        }
    }

    /*--- Phase 3: Basic variables — implied bound analysis ---*/
    MatrixData *mat = model->matrix;
    if (basis->basic_vars && mat && mat->row_ptr) {
        for (int i = 0; i < m; i++) {
            int bvar = basis->basic_vars[i];
            if (bvar < 0 || bvar >= n) continue;

            int result = analyze_basic(state, mat, i, bvar, feas_tol);
            if (result == -1) {
                state->problem_row_index = i;
                state->perturb_count += perturbed;
                return CXF_INFEASIBLE;
            }
            if (result == 1) {
                /* P2.6 Case B: degenerate basic variable —
                 * mark dirty in pricing to exclude from
                 * next candidate retrieval */
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, bvar);
                perturbed++;
            }
        }
    }

    /*--- Phase 4: Pricing notification ---*/
    if (state->pricing && perturbed > 0)
        cxf_pricing_end_level(state->pricing);

    if (state->work_counter)
        *state->work_counter += (double)total;

    state->perturb_count += perturbed;
    return CXF_OK;
}

/**
 * @brief Restore original bounds after perturbation (v2 P2.6).
 *
 * Restores from saved bounds (if available) or from model bounds.
 * Resets perturbation counter. Cleanup iterations are handled by
 * cxf_simplex_refine.
 */
int cxf_simplex_unperturb(SolverState *state, CxfEnv *env) {
    if (!state || !env) return CXF_ERROR_NULL_ARGUMENT;
    if (state->perturb_count == 0) return 1;

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;

    /* Prefer saved bounds (includes auxiliaries) over model bounds */
    if (state->saved_lb && state->saved_ub) {
        memcpy(state->work_lb, state->saved_lb,
               (size_t)total * sizeof(double));
        memcpy(state->work_ub, state->saved_ub,
               (size_t)total * sizeof(double));
    } else if (n > 0 && state->model_ref) {
        CxfModel *model = state->model_ref;
        if (state->work_lb && model->lb)
            memcpy(state->work_lb, model->lb, (size_t)n * sizeof(double));
        if (state->work_ub && model->ub)
            memcpy(state->work_ub, model->ub, (size_t)n * sizeof(double));
    }

    state->perturb_count = 0;
    return CXF_OK;
}
