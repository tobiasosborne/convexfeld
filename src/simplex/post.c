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

extern int cxf_basis_refactor(BasisState *basis);

/**
 * Post-iteration housekeeping
 *
 * Updates work counter and triggers basis refactorization when needed.
 *
 * @param state Solver state
 * @param env Environment with refactor_interval
 * @return 0 if continue normally, 1 if refactor triggered,
 *         CXF_ERROR_NULL_ARGUMENT if NULL pointers
 */
int cxf_simplex_post_iterate(SolverContext *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Update work counter if tracking is enabled */
    if (state->work_counter != NULL) {
        *state->work_counter += 1.0;
    }

    /* Check if refactorization is needed */
    if (state->eta_count >= env->refactor_interval) {
        int status = cxf_basis_refactor(state->basis);
        if (status != CXF_OK) {
            return status;
        }
        state->eta_count = 0;
        return 1; /* Refactor triggered */
    }

    return 0; /* Continue normally */
}

/**
 * Phase I to Phase II transition
 *
 * Checks feasibility, restores original objective, and transitions to Phase II.
 *
 * @param state Solver state
 * @param env Environment with feasibility_tol
 * @return CXF_OK if transition successful,
 *         CXF_INFEASIBLE if Phase I failed to find feasible solution,
 *         CXF_ERROR_NULL_ARGUMENT if NULL pointers
 */
int cxf_simplex_phase_end(SolverContext *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Only process if we're ending Phase I */
    if (state->phase != 1) {
        return CXF_OK;
    }

    /* Check feasibility: Phase I objective should be ~0 */
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

    return CXF_OK;
}
