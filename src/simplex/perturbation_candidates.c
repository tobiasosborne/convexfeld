/**
 * @file perturbation_candidates.c
 * @brief EXPAND perturbation candidate validation and processing.
 *
 * Extracted from perturbation.c to stay under 200 LOC limit.
 * Phase 2b: pre-perturbation consistency check.
 * Phase 4: nonbasic Case A + basic Case B processing.
 *
 * Spec: docs/specs-v2/specs/algorithms/perturbation.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#include "simplex_internal.h"

/** @brief Validate candidate list against solver state (Phase 2b). */
int cxf_perturb_validate_candidates(int *cand_list, int cand_count,
                                    BasisState *basis, int m, int n,
                                    int total) {
    int valid = 0;
    for (int ci = 0; ci < cand_count; ci++) {
        int j = cand_list[ci];
        if (j < 0 || j >= total) continue;
        if (!basis->var_status) continue;
        int s = basis->var_status[j];
        /* Keep only: nonbasic at lower, or basic with valid row */
        if (s == CXF_VAR_AT_LOWER ||
            (s >= 0 && s < m && j < n))
            cand_list[valid++] = j;
    }
    return valid;
}

/** @brief Process V2 pricing candidates (Phase 4, V2 path). */
static int process_v2_candidates(SolverState *state, BasisState *basis,
                                 int *cand_list, int cand_count,
                                 double feas_tol, int n, int m) {
    double *dj = state->work_dj;
    int perturbed = 0;

    for (int ci = 0; ci < cand_count; ci++) {
        int j = cand_list[ci];
        int status = basis->var_status[j];

        if (status == CXF_VAR_AT_LOWER && dj) {
            /* Case A: Nonbasic at lower bound */
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];
            if (ub - lb < feas_tol) continue;

            double rc = dj[j];
            if (fabs(rc) <= feas_tol || rc < -feas_tol) {
                if (rc < -feas_tol && j >= n && (j - n) < m &&
                    state->work_sense) {
                    char sense = state->work_sense[j - n];
                    if (sense == '=' || sense == 'E') {
                        state->problem_row_index = j - n;
                        return -(perturbed + 1);
                    }
                }
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            }
        } else if (status >= 0) {
            /* Case B: Basic variable — implied bound analysis */
            int row = status;
            if (row < 0 || row >= m) continue;
            if (j >= n) continue;  /* skip auxiliaries */

            int result = cxf_expand_analyze_basic(state, row, j,
                                                  feas_tol);
            if (result == -1) {
                state->problem_row_index = row;
                return -(perturbed + 1);
            }
            if (result == 1) {
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            }
        }
    }
    return perturbed;
}

/** @brief Fallback full-scan candidate processing (Phase 4, no V2). */
static int process_fallback_scan(SolverState *state, BasisState *basis,
                                 double feas_tol, int n, int m,
                                 int total) {
    double *dj = state->work_dj;
    if (!dj) return 0;
    int perturbed = 0;

    for (int j = 0; j < total; j++) {
        int s = basis->var_status[j];
        if (s != CXF_VAR_AT_LOWER && !(s >= 0 && j < n)) continue;
        if (s == CXF_VAR_AT_LOWER) {
            if (state->work_ub[j] - state->work_lb[j] < feas_tol)
                continue;
            double rc = dj[j];
            if (fabs(rc) > feas_tol && rc >= -feas_tol) continue;
            if (rc < -feas_tol && j >= n && (j - n) < m &&
                state->work_sense) {
                char se = state->work_sense[j - n];
                if (se == '=' || se == 'E') {
                    state->problem_row_index = j - n;
                    return -(perturbed + 1);
                }
            }
            if (state->pricing)
                cxf_pricing_mark_dirty(state->pricing, j);
            perturbed++;
        } else {
            int row = s;
            if (row < 0 || row >= m) continue;
            int r = cxf_expand_analyze_basic(state, row, j, feas_tol);
            if (r == -1) {
                state->problem_row_index = row;
                return -(perturbed + 1);
            }
            if (r == 1) {
                if (state->pricing)
                    cxf_pricing_mark_dirty(state->pricing, j);
                perturbed++;
            }
        }
    }
    return perturbed;
}

/**
 * @brief Process perturbation candidates (Phase 4 entry point).
 *
 * @return Number of variables perturbed (>= 0), or negative if
 *         infeasible. On infeasibility, the absolute value minus 1
 *         gives the count perturbed before detection. Caller must
 *         add to state->perturb_count and return CXF_INFEASIBLE.
 */
int cxf_perturb_process_candidates(SolverState *state, BasisState *basis,
                                   int *cand_list, int cand_count,
                                   double feas_tol, int n, int m,
                                   int total) {
    if (cand_count > 0 && cand_list != NULL)
        return process_v2_candidates(state, basis, cand_list,
                                     cand_count, feas_tol, n, m);
    if (state->work_dj)
        return process_fallback_scan(state, basis, feas_tol, n, m,
                                     total);
    return 0;
}
