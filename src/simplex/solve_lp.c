/**
 * @file solve_lp.c
 * @brief Main LP solver entry point — v2 unified loop (P3.25)
 *
 * Coordinates the simplex solve with a single iteration loop
 * handling both Phase I (feasibility) and Phase II (optimality).
 * Phase transition is managed inline via feasibility checking.
 *
 * V2 flow: init → crash → phase_one_setup → unified loop → refine
 *
 * Spec: docs/specs-v2/specs/modules/simplex_phases.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* Stall threshold: consecutive degenerate pivots before EXPAND */
#define STALL_THRESHOLD 50

/* External declarations — lifecycle */
extern int cxf_simplex_init(CxfModel *model, SolverState **stateP);
extern void cxf_simplex_final(SolverState *state);
extern int cxf_extract_solution(SolverState *state, CxfModel *model);

/* External declarations — v2 simplex phases */
extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);
extern int cxf_simplex_refine(SolverState *state, CxfEnv *env);

/* External declarations — decomposed components */
extern int cxf_check_obvious_infeasibility(CxfModel *model);
extern int cxf_check_obvious_unboundedness(CxfModel *model);
extern int cxf_solve_unconstrained(CxfModel *model);
extern int cxf_setup_phase_one(SolverState *state);
extern int cxf_transition_to_phase_two(SolverState *state, CxfModel *model);
extern void cxf_compute_reduced_costs(SolverState *state);

/* External declarations — iteration + Phase I helpers (phase_loop.c) */
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_check_phase_one_end(SolverState *state, CxfModel *model,
                                   CxfEnv *env);

int cxf_solve_lp(CxfModel *model) {
    SolverState *state = NULL;
    int rc;

    /* Validation and special cases */
    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }
    if (model->num_constrs == 0) return cxf_solve_unconstrained(model);

    CxfEnv *env = model->env;
    if (env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->matrix == NULL || model->matrix->col_ptr == NULL) {
        model->status = CXF_ERROR_NOT_SUPPORTED;
        return CXF_ERROR_NOT_SUPPORTED;
    }

    /* Presolve: detect obvious infeasibility/unboundedness */
    if (cxf_check_obvious_infeasibility(model)) {
        model->status = CXF_INFEASIBLE;
        return CXF_INFEASIBLE;
    }
    if (cxf_check_obvious_unboundedness(model)) {
        model->status = CXF_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* Initialize solver state */
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { model->status = rc; return rc; }

    /* V2: Crash basis (P2.5) */
    cxf_simplex_crash(state, env);

    /* Phase I setup: artificial variables + surrogate objective */
    rc = cxf_setup_phase_one(state);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }
    cxf_compute_reduced_costs(state);

    /* ===== Unified iteration loop (v2 — P3.25) ===== */
    while (state->iteration < state->max_iterations) {
        /* Anti-cycling: Bland's rule fallback */
        if (!state->use_bland &&
            state->iteration > 3 * state->num_constrs)
            state->use_bland = 1;

        /* Single iteration step (pricing + FTRAN + ratio test + pivot) */
        int status = cxf_simplex_step(state, env);

        if (status == ITERATE_OPTIMAL) {
            if (state->phase == 1) {
                /* Phase I optimal → check feasibility, maybe transition */
                rc = cxf_check_phase_one_end(state, model, env);
                if (rc == CXF_OK) continue;    /* → Phase II, keep going */
                if (rc == 1) continue;          /* Improving dir exists */
                model->status = CXF_INFEASIBLE;
                break;
            }
            model->status = CXF_OPTIMAL;
            break;
        }
        if (status == ITERATE_UNBOUNDED) {
            model->status = CXF_UNBOUNDED; break;
        }
        if (status == ITERATE_INFEASIBLE) {
            model->status = CXF_INFEASIBLE; break;
        }
        if (status < 0) {
            model->status = status; break;
        }

        /* V2: Stall-triggered perturbation (P2.6 EXPAND) */
        if (state->degenerate_count > STALL_THRESHOLD)
            cxf_simplex_perturbation(state, env);
    }

    if (state->iteration >= state->max_iterations)
        model->status = CXF_ITERATION_LIMIT;

    /* Post-solve: unperturb, refine, extract */
    cxf_simplex_unperturb(state, env);
    cxf_simplex_refine(state, env);
    if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);

    cxf_simplex_final(state);
    return model->status;
}
