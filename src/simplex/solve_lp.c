/**
 * @file solve_lp.c
 * @brief Main LP solver — v2 unified loop (P3.25)
 *
 * V2 iteration sequence (per P3.20 Module-Level Notes):
 *   1. progress_snapshot
 *   2. simplex_iterate
 *   3. perturbation (if stalling)
 *   4. step (main pivot + BFRT)
 *   5. step2 (variable-side bound propagation)
 *   6. step3 (constraint-side bound propagation)
 *   7. phase_end (post-pivot only — T2.10)
 *   8. basis_diff (convergence detection)
 *   9. post_iterate (stall, stagnation, termination)
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
#define CUMULATIVE_STALL   200   /* perturbation.md: cumulative degenerate pivot cap */

/* Lifecycle */

/* Scaling (disabled — see TODO in solve flow) */

/* Simplex phases (P3.21) */

/* Presolve */

/* Iteration loop (P3.20) */

/* Phase I helpers */

/* Outer round limit is mode-dependent (binary part3_main_loop.c:69-88):
 *   primal (0)      →  5 rounds
 *   dual/auto (1,-1) → 100 rounds
 *   crossover (3,4)  →  10 rounds
 *   other            → 100 rounds (conservative default) */
static int max_outer_rounds(int method) {
    if (method == 0) return 5;
    if (method == 3 || method == 4) return 10;
    return 100;
}
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
    if (rc != CXF_OK) { model->status = rc; cxf_state_free(state); return rc; }
    cxf_compute_reduced_costs(state);

    /* Preprocess: fix near-bound variables (v2 P3.21 step 3) */
    cxf_simplex_preprocess(state, env);

    /* Compute activity bounds (v2 P3.21 step 4 — cxf_simplex_setup) */
    cxf_simplex_setup(state, env, 0, NULL);

    /* ===== Two-level iteration loop (v2 P3.25 Phase 6) ===== */
    int terminated = 0;
    int stall = 0;
    int snap_buf[CXF_SNAPSHOT_SIZE];

    int max_rounds = max_outer_rounds(env->method);
    for (int round = 0; round < max_rounds && !terminated; round++) {
        cxf_progress_snapshot(state, snap_buf);
        int inner_checks = 0;  /* convergence check count within this round */

        while (state->iteration < state->max_iterations) {
            /* Bland's rule removed — not in binary (T3.1).
             * Binary relies on perturbation + pricing escalation. */

            /* (1) Progress snapshot — taken at round start */
            /* (2) Progress logging + callback */
            cxf_simplex_iterate(model, state);

            /* Pre-pivot phase_end removed — binary calls phase_end once,
             * post-pivot only (T2.10). */
            int status;

            /* (4) Perturbation — reactive on stall/degeneracy only.
             * V2 solve_lp_core.md Phase 6 step 4: apply perturbation only
             * "if the EXPAND procedure determines that the solver is
             * stalling." Proactive early-iteration perturbation removed
             * per convexfeld-9ksl. */
            if (stall || state->degenerate_count > STALL_THRESHOLD ||
                state->cumulative_degenerate > CUMULATIVE_STALL) {
                cxf_simplex_perturbation(state, env);
                stall = 0;
                /* QA Q16: cumulative counter is NEVER reset — original adds
                 * perturbedCount cumulatively. Removing reset makes the
                 * second perturbation fire sooner on persistent degeneracy. */
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
                /* QA Q14: Probe before accepting UNBOUNDED.
                 * Refactorize + recompute to rule out stale reduced costs
                 * from degraded basis representation. */
                cxf_solver_refactor(state, env);
                cxf_recompute_xB(state);
                cxf_recompute_objective(state);
                cxf_compute_reduced_costs(state);
                /* Re-run step with fresh data — if still UNBOUNDED, confirm */
                status = cxf_simplex_step(state, env);
                if (status == ITERATE_UNBOUNDED) {
                    model->status = CXF_UNBOUNDED; terminated = 1; break;
                }
                /* False alarm — keep iterating with fresh basis */
                continue;
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

            /* (8) Basis diff — convergence detection (T3.2: modulo removed).
             * Binary checks every pass when stagnation detected.
             * iteration_mode set by post_iterate at refactor boundaries.
             * Threshold = max(0, k - k_0) * tau; k_0=5 grace period. */
            if (state->iteration_mode == 1) {
                inner_checks++;
                double progress = cxf_basis_diff(state, snap_buf);
                int grace = inner_checks - 5;
                double threshold = (grace > 0)
                    ? (double)grace * CONVERGENCE_BASE : 0.0;
                if (progress < threshold) {
                    state->iteration_mode = 0;
                    break;
                }
                state->iteration_mode = 0;
                cxf_progress_snapshot(state, snap_buf);
            }

            /* (10) Post-iterate: stall, stagnation, termination */
            status = cxf_simplex_post_iterate(model, state, &stall);
            if (status == CXF_ITERATION_LIMIT) {
                model->status = CXF_ITERATION_LIMIT;
                terminated = 1; break;
            }
            if (CXF_IS_ERROR(status)) {
                model->status = status; terminated = 1; break;
            }
        }

        /* Outer-loop convergence (T3.2): if convergence was actively
         * tested this round (inner_checks > 0) and the basis still shows
         * no progress since round start, stop. The inner_checks guard
         * prevents premature termination during perturbation setup rounds
         * where the convergence path hasn't fired yet. */
        if (!terminated && inner_checks > 0 &&
            state->iteration < state->max_iterations) {
            double round_progress = cxf_basis_diff(state, snap_buf);
            if (round_progress <= 0.0)
                terminated = 1;
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

    /* P6.2: Dual-feasibility variable fixing (simplex_lifecycle.md).
     * 5-phase algorithm: target determination, equality verification,
     * activity propagation, constraint feasibility, apply fixings.
     * Replaces simple RC-sign snap with constraint-verified fixing. */
    cxf_simplex_final(state, env, NULL);

    /* Extract solution for all terminal statuses, not just OPTIMAL.
     * Iteration-limit and time-limit should still provide best-available. */
    if (model->status == CXF_OPTIMAL ||
        model->status == CXF_ITERATION_LIMIT ||
        model->status == CXF_TIME_LIMIT)
        cxf_extract_solution(state, model);

    /* P6.5: Post-solve implied-bound tightening and resource deallocation
     * (simplex_lifecycle.md step 9: cxf_simplex_cleanup). Must run after
     * cxf_simplex_final (dual-feasibility fixing) and before env restore. */
    cxf_simplex_cleanup(state, env);

    cxf_state_free(state);

    /* P6.4: Restore environment parameters */
    env->feasibility_tol = saved_feas_tol;
    env->optimality_tol = saved_opt_tol;
    env->refactor_interval = saved_refactor_interval;
    env->max_eta_count = saved_max_eta;

    return model->status;
}
