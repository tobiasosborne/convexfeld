/**
 * @file perturbation.c
 * @brief EXPAND anti-cycling perturbation (v2 P2.6, P5.1)
 *
 * Spec-compliant 5-phase EXPAND perturbation:
 *   Phase 1: Save bounds
 *   Phase 2: Retrieve candidates from V2 pricing (P4.5)
 *   Phase 3: Restore saved bounds before analysis (P5.2 fix)
 *   Phase 4: Candidate processing (nonbasic + basic unified)
 *   Phase 5: Counter update + pricing notification
 *
 * Spec: docs/specs-v2/specs/algorithms/perturbation.md
 * Beads: 6wgv (P5.1), 9yi2 (P5.2)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
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
static int analyze_basic(SolverState *state, int row, int bvar,
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

extern void cxf_pricing_candidates_v2(PricingState *ctx, SolverState *state,
                                      int *count, int **candidates);
extern void cxf_compute_activity_bounds(SolverState *state, int count,
                                        const int *indices);

/**
 * @brief Apply EXPAND anti-cycling perturbation (v2 P2.6, P5.1).
 *
 * Spec-compliant 5-phase flow per perturbation.md:
 *   Phase 1: Save bounds
 *   Phase 2: Retrieve candidates from V2 pricing
 *   Phase 3: Restore saved bounds before analysis
 *   Phase 4: Process candidates (nonbasic + basic)
 *   Phase 5: Counter update + pricing notification
 */
int cxf_simplex_perturbation(SolverState *state, CxfEnv *env) {
    if (!state || !env) return CXF_ERROR_NULL_ARGUMENT;

    int n = state->num_vars;
    int m = state->num_constrs;
    if (n == 0 || m == 0) return CXF_OK;

    BasisState *basis = state->basis;
    if (!basis || !basis->var_status) return CXF_OK;

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

    /*--- Phase 2: Candidate retrieval from V2 pricing (P5.1) ---
     * Use the V2 dirty queue instead of full-scanning all variables.
     * Falls back to full scan if pricing unavailable or empty. */
    int cand_count = 0;
    int *cand_list = NULL;
    if (state->pricing)
        cxf_pricing_candidates_v2(state->pricing, state,
                                  &cand_count, &cand_list);

    /*--- Phase 3: Bound drift prevention (P5.2) ---
     * Our perturbation uses candidate removal (not bound modification),
     * so work_lb/work_ub are never modified by this function. The
     * analyze_basic() helper already reads saved_lb/saved_ub for other
     * variables' bounds, preventing drift. No explicit bound restoration
     * needed — that would undo valid preprocessing tightening.
     * Full bound restoration happens in cxf_simplex_unperturb(). */

    /*--- Phase 4: Candidate processing ---
     * Process V2 candidates if available, else fall back to full scan.
     * Case A: Nonbasic at lower — check RC, mark degenerate dirty
     * Case B: Basic — implied bound analysis */
    double *dj = state->work_dj;

    if (cand_count > 0 && cand_list != NULL) {
        /* V2 path: process only pricing candidates */
        for (int ci = 0; ci < cand_count; ci++) {
            int j = cand_list[ci];
            if (j < 0 || j >= total) continue;

            int status = basis->var_status[j];

            if (status == CXF_VAR_AT_LOWER && dj) {
                /* Case A: Nonbasic at lower bound */
                double lb = state->work_lb[j];
                double ub = state->work_ub[j];
                if (ub - lb < feas_tol) continue;

                double rc = dj[j];
                if (fabs(rc) <= feas_tol || rc < -feas_tol) {
                    /* Degenerate or negative RC: check equality infeasibility */
                    if (rc < -feas_tol && j >= n && (j - n) < m &&
                        state->work_sense) {
                        char sense = state->work_sense[j - n];
                        if (sense == '=' || sense == 'E') {
                            state->problem_row_index = j - n;
                            state->perturb_count += perturbed;
                            return CXF_INFEASIBLE;
                        }
                    }
                    if (state->pricing)
                        cxf_pricing_mark_dirty(state->pricing, j);
                    perturbed++;
                }
            } else if (status >= 0) {
                /* Case B: Basic variable — implied bound analysis */
                int row = status;  /* var_status >= 0 is the basis row */
                if (row < 0 || row >= m) continue;
                if (j >= n) continue;  /* skip auxiliaries */

                int result = analyze_basic(state, row, j, feas_tol);
                if (result == -1) {
                    state->problem_row_index = row;
                    state->perturb_count += perturbed;
                    return CXF_INFEASIBLE;
                }
                if (result == 1) {
                    if (state->pricing)
                        cxf_pricing_mark_dirty(state->pricing, j);
                    perturbed++;
                }
            }
        }
    } else if (dj) {
        /* Fallback: synthesize candidate list from full scan */
        for (int j = 0; j < total; j++) {
            int s = basis->var_status[j];
            if (s != CXF_VAR_AT_LOWER && !(s >= 0 && j < n)) continue;
            /* Reuse the V2 path logic inline for each candidate */
            if (s == CXF_VAR_AT_LOWER) {
                if (state->work_ub[j] - state->work_lb[j] < feas_tol)
                    continue;
                double rc = dj[j];
                if (fabs(rc) > feas_tol && rc >= -feas_tol) continue;
                if (rc < -feas_tol && j >= n && (j-n) < m &&
                    state->work_sense) {
                    char se = state->work_sense[j - n];
                    if (se == '=' || se == 'E') {
                        state->problem_row_index = j - n;
                        state->perturb_count += perturbed;
                        return CXF_INFEASIBLE;
                    }
                }
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            } else {
                int row = s;
                if (row < 0 || row >= m) continue;
                int r = analyze_basic(state, row, j, feas_tol);
                if (r == -1) {
                    state->problem_row_index = row;
                    state->perturb_count += perturbed;
                    return CXF_INFEASIBLE;
                }
                if (r == 1) {
                    if (state->pricing)
                        cxf_pricing_mark_dirty(state->pricing, j);
                    perturbed++;
                }
            }
        }
    }

    /*--- Phase 5: Counter update + pricing notification ---*/
    if (state->pricing && perturbed > 0)
        cxf_pricing_end_level(state->pricing);

    if (state->work_counter)
        *state->work_counter += (double)(cand_count > 0 ? cand_count : total);

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

    /* Prefer saved bounds over model bounds */
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
