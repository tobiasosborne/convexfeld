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
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"

#define STALL_THRESHOLD    50

/* Lifecycle */

/* Scaling (disabled — see TODO in solve flow) */

/* Simplex phases (P3.21) */

/* Presolve */

/* Iteration loop (P3.20) */

/* Phase I helpers */

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

    /* Complete row classification after crash.
     * Crash returns early on infeasible rows (spec postcondition 2),
     * leaving subsequent rows UNASSIGNED. Mark remaining rows
     * BASIC_LOWER to satisfy spec postcondition 1 (all unassigned
     * rows classified) and enable future crash improvements. */
    if (state->row_status) {
        for (int i = 0; i < state->num_constrs; i++) {
            if (state->row_status[i] == CXF_ROW_UNASSIGNED)
                state->row_status[i] = CXF_ROW_BASIC_LOWER;
        }
    }

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
        int inner_checks = 0;  /* convergence check count within this round */

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

            /* (4) Perturbation — reactive on stall/degeneracy only.
             * V2 solve_lp_core.md Phase 6 step 4: apply perturbation only
             * "if the EXPAND procedure determines that the solver is
             * stalling." Proactive early-iteration perturbation removed
             * per convexfeld-9ksl. */
            if (stall || state->degenerate_count > STALL_THRESHOLD) {
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
                /* P6.3b: Verify optimality with fresh data before accepting.
                 * Numerical drift can make all RCs appear non-negative when
                 * improving directions still exist (numerical_stability.md
                 * Section F.6). Refactorize, recompute, re-check. */
                cxf_solver_refactor(state, env);
                cxf_recompute_xB(state);
                cxf_recompute_objective(state);
                cxf_compute_reduced_costs(state);
                {
                    int verified = 1;
                    int total_v = state->num_vars + state->num_constrs;
                    for (int j = 0; j < total_v; j++) {
                        if (state->basis->var_status[j] >= 0) continue;
                        double range = state->work_ub[j] - state->work_lb[j];
                        if (range < CXF_FEASIBILITY_TOL) continue;
                        double dj = state->work_dj[j];
                        if (state->basis->var_status[j] == CXF_VAR_AT_LOWER
                            && dj < -env->optimality_tol)
                            { verified = 0; break; }
                        if (state->basis->var_status[j] == CXF_VAR_AT_UPPER
                            && dj > env->optimality_tol)
                            { verified = 0; break; }
                        if (state->basis->var_status[j] == CXF_VAR_SUPERBASIC
                            && fabs(dj) > env->optimality_tol)
                            { verified = 0; break; }
                    }
                    if (!verified) continue;  /* False optimal — keep iterating */
                }
                model->status = CXF_OPTIMAL;
                terminated = 1; break;
            }
            if (status == ITERATE_UNBOUNDED) {
                if (state->phase == 1) {
                    /* Phase I unbounded: refactorize and continue.
                     * Unboundedness of auxiliary problem does not imply
                     * unboundedness of original (two_phase_method.md). */
                    cxf_solver_refactor(state, env);
                    cxf_recompute_xB(state);
                    cxf_recompute_objective(state);
                    cxf_compute_reduced_costs(state);
                    continue;
                }
                model->status = CXF_UNBOUNDED; terminated = 1; break;
            }
            if (status == ITERATE_INFEASIBLE) {
                model->status = CXF_INFEASIBLE; terminated = 1; break;
            }
            if (CXF_IS_ERROR(status)) {
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

            /* (9) Basis diff — convergence detection (spec: perturbation.md).
             * Threshold = max(0, k - k_0) * tau where k = inner check count,
             * k_0 = 5 (grace period), tau = CONVERGENCE_BASE.
             * First 5 checks: threshold=0 → any nonneg progress continues.
             * After grace: threshold grows linearly → increasingly impatient. */
            if (state->iteration_mode == 1 &&
                state->iteration > 0 &&
                state->iteration % (state->num_constrs + 1) == 0) {
                inner_checks++;
                double progress = cxf_basis_diff(state);
                int grace = inner_checks - 5;
                double threshold = (grace > 0)
                    ? (double)grace * CONVERGENCE_BASE : 0.0;
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
            if (CXF_IS_ERROR(status)) {
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
        cxf_simplex_postsolve(state, env);
    }

    /* P6.3: Final accuracy pass (numerical_stability.md).
     * Force refactorization + from-scratch x_B and obj recomputation
     * to eliminate all accumulated drift before reporting the answer.
     * Runs for OPTIMAL, ITERATION_LIMIT, and TIME_LIMIT — any status
     * where a solution will be extracted needs accurate primal values. */
    if (model->status == CXF_OPTIMAL ||
        model->status == CXF_ITERATION_LIMIT ||
        model->status == CXF_TIME_LIMIT) {
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

    /* P6.2: Complementary slackness fix — snap nonbasic variables to
     * the correct bound based on reduced cost sign. Must run BEFORE
     * extract so the solution includes CS corrections. */
    if (state->basis && state->basis->var_status &&
        state->work_dj && state->work_x) {
        int total_cs = state->num_vars + state->num_constrs;
        for (int j = 0; j < total_cs; j++) {
            int vs = state->basis->var_status[j];
            if (vs >= 0) continue;  /* skip basic */
            double dj = state->work_dj[j];
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];
            /* CS: if dj > 0, should be at lower; if dj < 0, at upper */
            if (dj > CXF_OPTIMALITY_TOL && lb > -CXF_INFINITY)
                state->work_x[j] = lb;
            else if (dj < -CXF_OPTIMALITY_TOL && ub < CXF_INFINITY)
                state->work_x[j] = ub;
        }
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
