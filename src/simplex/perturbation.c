/**
 * @file perturbation.c
 * @brief EXPAND anti-cycling perturbation — Mechanism A + orchestration.
 *
 * 5-phase flow: save bounds, V2 candidate retrieval, drift prevention,
 * candidate processing, counter update. Mechanism B in expand.c.
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
#include <string.h>

#include "simplex_internal.h"

/** @brief Apply EXPAND anti-cycling perturbation (v2 P2.6, P5.1). */
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

    /*--- Phase 2: Candidate retrieval from V2 pricing (P5.1) ---*/
    int cand_count = 0;
    int *cand_list = NULL;
    if (state->pricing)
        cxf_pricing_candidates(state->pricing, state,
                                  &cand_count, &cand_list);

    /* Phase 2b: Pre-perturbation consistency check (perturbation.md Phase 2).
     * Validate candidate list against current solver state before processing.
     * Discard invalid candidates up-front to avoid stale data issues. */
    if (cand_count > 0 && cand_list != NULL)
        cand_count = cxf_perturb_validate_candidates(
            cand_list, cand_count, basis, m, n, total);

    /*--- Phase 3: Bound restoration (diagnostic mode, perturbation.md Phase 3).
     * In detailed diagnostic mode (verbosity >= 2), copy saved bounds into
     * working arrays so perturbation analysis runs against unperturbed data.
     * After copying, recompute constraint activities. In standard mode,
     * perturbation operates on current working bounds directly. ---*/
    if (env->verbosity >= 2 && state->perturb_count > 0 &&
        state->saved_lb && state->saved_ub) {
        memcpy(state->work_lb, state->saved_lb,
               (size_t)total * sizeof(double));
        memcpy(state->work_ub, state->saved_ub,
               (size_t)total * sizeof(double));
        cxf_compute_activity_bounds(state, 0, NULL);
    }

    /* Phase 4: candidate processing (nonbasic Case A + basic Case B).
     * Delegated to perturbation_candidates.c. Returns >= 0 for count,
     * or negative on infeasibility (abs value - 1 = count before). */
    int phase4 = cxf_perturb_process_candidates(
        state, basis, cand_list, cand_count, feas_tol, n, m, total);
    if (phase4 < 0) {
        state->perturb_count += (-(phase4 + 1));
        return CXF_INFEASIBLE;
    }
    perturbed = phase4;

    /* T2.12: Binary has no Mechanism B (EXPAND bound widening is fabricated).
     * Only pricing exclusion (Mechanism A) exists in the binary. */

    /*--- Phase 5: Counter update + pricing notification ---*/
    if (state->pricing && perturbed > 0)
        cxf_pricing_end_level(state->pricing, state);

    if (state->work_counter)
        *state->work_counter += (double)(cand_count > 0 ? cand_count : total);

    state->perturb_count += perturbed;
    return CXF_OK;
}

/** @brief Restore original bounds after perturbation (v2 P2.6). */
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
    state->perturb_expand_active = 0;
    state->mechanism_a_applied = 0;
    return CXF_OK;
}
