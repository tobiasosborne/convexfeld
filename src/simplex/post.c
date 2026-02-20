/**
 * @file post.c
 * @brief Post-iteration and phase transition functions (M7.1.11)
 *
 * cxf_simplex_post_iterate: Housekeeping after each pivot
 * cxf_simplex_phase_end: Phase I to Phase II transition
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

extern int cxf_fix_variables_at_bounds(BasisState *basis);

/* Default stall detection parameters (v2 P3.20) */
#define STALL_ALPHA 0.5
#define STALL_BETA  3

/**
 * @brief Post-iteration monitoring (v2 P3.20).
 *
 * Four checks:
 *   1. Stall detection (dimension-scaled progress thresholds)
 *   2. Solve status (iteration limit, time limit)
 *   3. Objective stagnation (delta < optimality_tol at refactor boundary)
 *   4. User interrupt (placeholder)
 *
 * @return 0 to continue, >0 termination code, <0 error
 */
int cxf_simplex_post_iterate(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Update work counter if tracking is enabled */
    if (state->work_counter != NULL) {
        *state->work_counter += 1.0;
    }

    /* Check 1: Stall detection — only at refactor boundaries */
    if (state->basis != NULL &&
        state->eta_count >= env->refactor_interval) {
        int n = state->num_vars;
        int m = state->num_constrs;

        /* Column progress: cols_eliminated <= alpha*n + beta */
        int col_threshold = (int)(STALL_ALPHA * n + STALL_BETA);
        int col_stall = (state->cols_eliminated > col_threshold);

        /* Row progress: rows_eliminated <= beta + alpha*M_eff */
        int m_eff = m + state->bounds_propagated;
        int row_threshold = (int)(STALL_BETA + STALL_ALPHA * m_eff);
        int row_stall = (state->rows_eliminated > row_threshold);

        if (col_stall || row_stall) {
            state->iteration_mode = 1;  /* Signal stagnation */
        }

        /* Check 3: Objective stagnation at refactor boundary */
        double delta_z = state->obj_value - state->obj_at_last_refactor;
        if (delta_z < env->optimality_tol && delta_z != env->optimality_tol) {
            state->iteration_mode = 1;  /* Stagnation */
        }

        /* Record current objective for next refactor interval */
        state->obj_at_last_refactor = state->obj_value;

        /* Trigger refactorization */
        int status = cxf_fix_variables_at_bounds(state->basis);
        if (status != CXF_OK) return status;
        state->eta_count = 0;

        /* Reset per-interval progress counters */
        state->rows_eliminated = 0;
        state->cols_eliminated = 0;
    }

    /* Check 2: Iteration limit */
    if (state->iteration >= state->max_iterations) {
        return CXF_ITERATION_LIMIT;
    }

    /* Check 4: User interrupt (placeholder — no signal handling yet) */

    return 0;
}

/**
 * @brief Phase-end processing (v2 P3.21).
 *
 * Called twice per iteration (pre-pivot and post-pivot).
 * Phase 1: constraint candidate processing (stub — needs pricing queue)
 * Phase 2: activity bound recomputation (stub — needs cxf_simplex_setup)
 * Also handles Phase I→II transition.
 *
 * @param state Solver state
 * @param env   Environment with tolerances
 * @return 0 on success, CXF_INFEASIBLE on dual infeasibility
 */
int cxf_simplex_phase_end(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Phase I→II transition: check feasibility */
    if (state->phase != 1) {
        return CXF_OK;
    }

    if (state->obj_value > env->feasibility_tol) {
        return CXF_INFEASIBLE;
    }

    /* Restore original objective coefficients */
    CxfModel *model = state->model_ref;
    if (model == NULL || model->obj_coeffs == NULL || state->work_obj == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    for (int j = 0; j < model->num_vars; j++) {
        state->work_obj[j] = model->obj_coeffs[j];
    }

    /* Recompute objective value with original objective */
    double obj_val = 0.0;
    if (state->work_x != NULL) {
        for (int j = 0; j < model->num_vars; j++) {
            obj_val += state->work_obj[j] * state->work_x[j];
        }
    }
    state->obj_value = obj_val;

    /* Transition to Phase II */
    state->phase = 2;

    /* TODO (D1): constraint candidate processing via pricing queue
     * TODO (D1): activity bound recomputation for modified constraints */

    return CXF_OK;
}
