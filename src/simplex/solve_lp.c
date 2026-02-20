/**
 * @file solve_lp.c
 * @brief Main LP solver entry point — v2 unified loop (P3.25)
 *
 * Coordinates the simplex solve with a single iteration loop
 * handling both Phase I (feasibility) and Phase II (optimality).
 * Phase transition is managed inline via feasibility checking.
 *
 * V2 flow: init → crash → setup → preprocess → phase_one_setup → loop → refine
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

/* External declarations — v2 iteration loop (P3.20) */
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_simplex_step2(SolverState *state, CxfEnv *env);
extern int cxf_simplex_step3(SolverState *state, CxfEnv *env);
extern void cxf_log_iteration_progress(CxfModel *model, SolverState *state);
extern int cxf_simplex_post_iterate(SolverState *state, CxfEnv *env);
extern void cxf_progress_snapshot(SolverState *state);
extern double cxf_basis_diff(SolverState *state);

/* External declarations — Phase I helpers (phase_loop.c) */
extern int cxf_check_phase_one_end(SolverState *state, CxfModel *model,
                                   CxfEnv *env);

/* Two-level loop parameters (v2 P3.25 Phase 6) */
#define MAX_OUTER_ROUNDS    100
#define CONVERGENCE_BASE    0.01

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

    /* V2: Setup activity bounds (C1) + preprocess near-bound vars (C2)
     * NOTE: cxf_simplex_setup resets iteration counter and pricing.
     * Skipped for now — setup is already done by cxf_simplex_init +
     * the implicit setup in phase_one_setup. Activity bounds are
     * computed but the full setup reset is deferred. */
    /* cxf_simplex_setup(state, env); */
    /* cxf_simplex_preprocess(state, env, 0); */

    /* Phase I setup: artificial variables + surrogate objective */
    rc = cxf_setup_phase_one(state);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }
    cxf_compute_reduced_costs(state);

    /* ===== Two-level iteration loop (v2 — P3.25 Phase 6) ===== */
    int terminated = 0;

    for (int round = 0; round < MAX_OUTER_ROUNDS && !terminated; round++) {
        /* Capture baseline for convergence detection */
        cxf_progress_snapshot(state);

        /* ----- Inner loop: run iterations until convergence or limit ----- */
        while (state->iteration < state->max_iterations) {
            /* Anti-cycling: Bland's rule fallback */
            if (!state->use_bland &&
                state->iteration > 3 * state->num_constrs)
                state->use_bland = 1;

            /* (1) Progress snapshot (already taken at round start) */
            /* (2) Progress logging + callback */
            cxf_log_iteration_progress(model, state);

            /* (3) Stall-triggered perturbation (P2.6 EXPAND) */
            if (state->degenerate_count > STALL_THRESHOLD)
                cxf_simplex_perturbation(state, env);

            /* (4) Main simplex pivot */
            int status = cxf_simplex_step(state, env);

            if (status == ITERATE_OPTIMAL) {
                if (state->phase == 1) {
                    rc = cxf_check_phase_one_end(state, model, env);
                    if (rc == CXF_OK) continue;
                    if (rc == 1) continue;
                    model->status = CXF_INFEASIBLE;
                    terminated = 1;
                    break;
                }
                model->status = CXF_OPTIMAL;
                terminated = 1;
                break;
            }
            if (status == ITERATE_UNBOUNDED) {
                model->status = CXF_UNBOUNDED; terminated = 1; break;
            }
            if (status == ITERATE_INFEASIBLE) {
                model->status = CXF_INFEASIBLE; terminated = 1; break;
            }
            if (status < 0) {
                model->status = status; terminated = 1; break;
            }

            /* (5) Variable-side bound propagation */
            status = cxf_simplex_step2(state, env);
            if (status != 0) {
                model->status = status; terminated = 1; break;
            }

            /* (6) Constraint-side bound propagation (LP only) */
            status = cxf_simplex_step3(state, env);
            if (status != 0) {
                model->status = status; terminated = 1; break;
            }

            /* (7) Phase-end check (Phase II only — Phase I handled above) */
            if (state->phase == 2) {
                status = cxf_simplex_phase_end(state, env);
                if (status == CXF_INFEASIBLE) {
                    model->status = CXF_INFEASIBLE; terminated = 1; break;
                }
            }

            /* (8) Basis diff — convergence detection at stagnation */
            if (state->iteration_mode == 1 &&
                state->iteration > 0 &&
                state->iteration % (state->num_constrs + 1) == 0) {
                double progress = cxf_basis_diff(state);
                double threshold = CONVERGENCE_BASE / (1.0 + round);
                if (progress < threshold) {
                    state->iteration_mode = 0;  /* Reset flag */
                    break;  /* Inner loop converged → next outer round */
                }
                state->iteration_mode = 0;
                cxf_progress_snapshot(state);
            }

            /* (9) Post-iterate: stall, stagnation, termination */
            status = cxf_simplex_post_iterate(state, env);
            if (status == CXF_ITERATION_LIMIT) {
                model->status = CXF_ITERATION_LIMIT;
                terminated = 1; break;
            }
            if (status < 0) {
                model->status = status; terminated = 1; break;
            }

            /* Snapshot taken inside convergence check above */
        }

        /* Check outer-loop termination: iteration limit */
        if (!terminated && state->iteration >= state->max_iterations) {
            model->status = CXF_ITERATION_LIMIT;
            terminated = 1;
        }
    }

    /* Post-solve: unperturb, refine, extract */
    cxf_simplex_unperturb(state, env);
    cxf_simplex_refine(state, env);
    if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);

    cxf_simplex_final(state);
    return model->status;
}
