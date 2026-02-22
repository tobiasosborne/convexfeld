/**
 * @file solve_lp.c
 * @brief Main LP solver — v2 unified loop (P3.25)
 *
 * V2 iteration sequence (per P3.20 Module-Level Notes):
 *   1. progress_snapshot
 *   2. log_iteration_progress
 *   3. phase_end (pre-pivot)
 *   4. perturbation (if stalling)
 *   5. step (main pivot + BFRT)
 *   6. step2 (variable-side bound propagation)
 *   7. step3 (constraint-side bound propagation)
 *   8. phase_end (post-pivot)
 *   9. basis_diff (convergence detection)
 *  10. post_iterate (stall, stagnation, termination)
 *
 * Spec: docs/specs-v2/specs/modules/solve_lp_core.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3
#define STALL_THRESHOLD    50

/* Lifecycle */
extern int cxf_simplex_init(CxfModel *model, SolverState **stateP);
extern void cxf_simplex_final(SolverState *state);
extern int cxf_extract_solution(SolverState *state, CxfModel *model);

/* Scaling (disabled — see TODO in solve flow) */

/* Simplex phases (P3.21) */
extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);
extern int cxf_simplex_refine(SolverState *state, CxfEnv *env);
extern int cxf_simplex_phase_end(SolverState *state, CxfEnv *env,
                                 int doScan);

/* Presolve */
extern int cxf_check_obvious_infeasibility(CxfModel *model);
extern int cxf_check_obvious_unboundedness(CxfModel *model);
extern int cxf_solve_unconstrained(CxfModel *model);
extern int cxf_setup_phase_one(SolverState *state);
extern int cxf_compute_reduced_costs(SolverState *state);

/* Iteration loop (P3.20) */
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_simplex_step2(SolverState *state, CxfEnv *env);
extern int cxf_simplex_step3(SolverState *state, CxfEnv *env);
extern void cxf_log_iteration_progress(CxfModel *model, SolverState *state);
extern int cxf_simplex_post_iterate(SolverState *state, CxfEnv *env,
                                    int *outStall);
extern void cxf_progress_snapshot(SolverState *state);
extern double cxf_basis_diff(SolverState *state);

/* Phase I helpers */
extern int cxf_check_phase_one_end(SolverState *state, CxfModel *model,
                                   CxfEnv *env);

#define MAX_OUTER_ROUNDS    100
#define CONVERGENCE_BASE    0.01

int cxf_solve_lp(CxfModel *model) {
    SolverState *state = NULL;
    int rc;

    /* Validation */
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

    /* P6.4: Save environment parameters for restore after solve */
    double saved_feas_tol = env->feasibility_tol;
    double saved_opt_tol = env->optimality_tol;
    int saved_refactor_interval = env->refactor_interval;
    int saved_max_eta = env->max_eta_count;

    /* Presolve */
    if (cxf_check_obvious_infeasibility(model)) {
        model->status = CXF_INFEASIBLE; return CXF_INFEASIBLE;
    }
    if (cxf_check_obvious_unboundedness(model)) {
        model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED;
    }

    /* Initialize solver state */
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { model->status = rc; return rc; }

    /* Full row+column Ruiz equilibration (matrix_finalization.md Strategy 3).
     * Must run AFTER init (copies matrix) but BEFORE crash/Phase I
     * (which use scaled data). */
    {
        extern void cxf_scale_problem(SolverState *, double *, double *);
        int m_s = state->num_constrs;
        int n_s = state->num_vars;
        double *rs = (double *)malloc((size_t)m_s * sizeof(double));
        double *cs = (double *)malloc((size_t)n_s * sizeof(double));
        if (rs && cs) {
            cxf_scale_problem(state, rs, cs);

            /* Check if scaling was actually applied */
            int scaled = 0;
            for (int i = 0; i < m_s && !scaled; i++)
                if (fabs(rs[i] - 1.0) > 1e-12) scaled = 1;
            for (int j = 0; j < n_s && !scaled; j++)
                if (fabs(cs[j] - 1.0) > 1e-12) scaled = 1;

            if (scaled) {
                state->row_scale = rs;
                state->col_scale = cs;
                /* Re-save bounds for EXPAND perturbation */
                int total_s = n_s + m_s;
                if (state->saved_lb && state->saved_ub) {
                    memcpy(state->saved_lb, state->work_lb,
                           (size_t)total_s * sizeof(double));
                    memcpy(state->saved_ub, state->work_ub,
                           (size_t)total_s * sizeof(double));
                }
            } else {
                free(rs);
                free(cs);
            }
        } else {
            free(rs);
            free(cs);
        }
    }

    /* Crash basis (P2.5) */
    cxf_simplex_crash(state, env);

    /* Phase I setup */
    rc = cxf_setup_phase_one(state);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }
    cxf_compute_reduced_costs(state);

    /* Compute activity bounds (v2 P3.21 — cxf_simplex_setup) */
    cxf_simplex_setup(state, env, 0, NULL);

    /* Preprocess: fix near-bound variables (v2 P3.21) */
    cxf_simplex_preprocess(state, env, 0);

    /* ===== Two-level iteration loop (v2 P3.25 Phase 6) ===== */
    int terminated = 0;
    int stall = 0;

    for (int round = 0; round < MAX_OUTER_ROUNDS && !terminated; round++) {
        cxf_progress_snapshot(state);

        while (state->iteration < state->max_iterations) {
            /* P5.4: Bland's rule is last resort, after perturbation fails.
             * Only activate if perturbation has been tried (perturb_count > 0)
             * AND we still have excessive degenerate pivots. */
            if (!state->use_bland &&
                state->perturb_count > 0 &&
                state->degenerate_count > 3 * state->num_constrs)
                state->use_bland = 1;

            /* (1) Progress snapshot — taken at round start */
            /* (2) Progress logging + callback */
            cxf_log_iteration_progress(model, state);

            /* (3) Pre-pivot phase_end — runs unconditionally per spec
             * P1.2 (fiyt): removed phase==2 guard. Spec simplex_iteration.md
             * item 7 calls phase_end in both phases. */
            int status = cxf_simplex_phase_end(state, env, 0);
            if (status == CXF_INFEASIBLE) {
                model->status = CXF_INFEASIBLE; terminated = 1; break;
            }

            /* (4) Perturbation — proactive in first 2 iters of round 0,
             * then reactive on stall/degeneracy. P1.6 (zr5l). */
            if (round == 0 && state->iteration <= 2) {
                cxf_simplex_perturbation(state, env);
            } else if (stall || state->degenerate_count > STALL_THRESHOLD) {
                cxf_simplex_perturbation(state, env);
                stall = 0;
            }

            /* (5) Main simplex pivot */
            status = cxf_simplex_step(state, env);

            if (status == ITERATE_OPTIMAL) {
                if (state->phase == 1) {
                    rc = cxf_check_phase_one_end(state, model, env);
                    if (rc == CXF_OK || rc == 1) continue;
                    model->status = CXF_INFEASIBLE;
                    terminated = 1; break;
                }
                model->status = CXF_OPTIMAL;
                terminated = 1; break;
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

            /* (6) Variable-side bound propagation */
            status = cxf_simplex_step2(state, env);
            if (status != 0) {
                model->status = status; terminated = 1; break;
            }

            /* (7) Constraint-side bound propagation (LP only) */
            status = cxf_simplex_step3(state, env);
            if (status != 0) {
                model->status = status; terminated = 1; break;
            }

            /* (8) Post-pivot phase_end — with doScan in Phase II */
            status = cxf_simplex_phase_end(state, env,
                                           (state->phase == 2));
            if (status == CXF_INFEASIBLE) {
                model->status = CXF_INFEASIBLE; terminated = 1; break;
            }

            /* (9) Basis diff — convergence detection */
            if (state->iteration_mode == 1 &&
                state->iteration > 0 &&
                state->iteration % (state->num_constrs + 1) == 0) {
                double progress = cxf_basis_diff(state);
                double threshold = CONVERGENCE_BASE / (1.0 + round);
                if (progress < threshold) {
                    state->iteration_mode = 0;
                    break;
                }
                state->iteration_mode = 0;
                cxf_progress_snapshot(state);
            }

            /* (10) Post-iterate: stall, stagnation, termination */
            status = cxf_simplex_post_iterate(state, env, &stall);
            if (status == CXF_ITERATION_LIMIT) {
                model->status = CXF_ITERATION_LIMIT;
                terminated = 1; break;
            }
            if (status < 0) {
                model->status = status; terminated = 1; break;
            }
        }

        if (!terminated && state->iteration >= state->max_iterations) {
            model->status = CXF_ITERATION_LIMIT;
            terminated = 1;
        }
    }

    /* Post-solve */
    cxf_simplex_unperturb(state, env);
    cxf_simplex_refine(state, env);

    /* P6.1: Postsolve — restore fixed variables and unscale (stub for now) */
    {
        extern int cxf_simplex_postsolve(SolverState *, CxfEnv *);
        cxf_simplex_postsolve(state, env);
    }

    /* P6.3: Final accuracy pass at OPTIMAL (numerical_stability.md).
     * Force refactorization + from-scratch x_B and obj recomputation
     * to eliminate all accumulated drift before reporting the answer. */
    if (model->status == CXF_OPTIMAL) {
        extern int cxf_recompute_xB(SolverState *state);
        extern void cxf_recompute_objective(SolverState *state);
        cxf_solver_refactor(state, env);
        cxf_recompute_xB(state);
        cxf_recompute_objective(state);
    }

    /* Unscale primal solution and restore original bounds.
     * Must run AFTER final accuracy pass (uses scaled matrix) but
     * BEFORE extract and CS fix in cxf_simplex_final (use original bounds). */
    if (state->col_scale != NULL) {
        for (int j = 0; j < state->num_vars; j++)
            state->work_x[j] *= state->col_scale[j];
        /* Restore original structural bounds for CS fix */
        if (model->lb && model->ub)
            memcpy(state->work_lb, model->lb,
                   (size_t)state->num_vars * sizeof(double));
        if (model->ub)
            memcpy(state->work_ub, model->ub,
                   (size_t)state->num_vars * sizeof(double));
    }

    /* Extract solution for all terminal statuses, not just OPTIMAL.
     * Iteration-limit and time-limit should still provide best-available. */
    if (model->status == CXF_OPTIMAL ||
        model->status == CXF_ITERATION_LIMIT ||
        model->status == CXF_TIME_LIMIT)
        cxf_extract_solution(state, model);

    cxf_simplex_final(state);

    /* P6.4: Restore environment parameters */
    env->feasibility_tol = saved_feas_tol;
    env->optimality_tol = saved_opt_tol;
    env->refactor_interval = saved_refactor_interval;
    env->max_eta_count = saved_max_eta;

    return model->status;
}
