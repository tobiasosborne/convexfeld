/**
 * @file post.c
 * @brief Post-iteration monitoring + phase-end processing (v2 P3.20/P3.21)
 *
 * cxf_simplex_post_iterate: Stall detection, termination, stagnation
 * cxf_simplex_phase_end:    Constraint cleanup + Phase I→II transition
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md (post_iterate)
 *       docs/specs-v2/specs/modules/simplex_phases.md (phase_end)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"


#define STALL_ALPHA 0.5
#define STALL_BETA  3

/*===========================================================================
 * cxf_simplex_post_iterate — Post-iteration monitoring (v2 P3.20)
 *
 * V2 spec signature: (CxfModel*, SolverState*, int *outStall) -> int
 * Derives env from model->env internally.
 *
 * Checks: (1) stall detection, (2) iteration limit,
 *         (3) objective stagnation, (4) user interrupt.
 *===========================================================================*/

int cxf_simplex_post_iterate(CxfModel *model, SolverState *state,
                             int *outStall) {
    if (model == NULL || state == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    CxfEnv *env = model->env;
    if (env == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    if (state->work_counter)
        *state->work_counter += 1.0;

    /* Check 1: Stall detection — only at refactor boundaries.
     * V2 simplex_iteration.md: stall detection runs at refactorization
     * boundaries. Use same adaptive threshold as cxf_refactor_check
     * (min(100, max(50, m/4)), reduced by residual-triggered adaptation)
     * so post_iterate and step.c agree on when refactorization occurs. */
    int eta_limit = state->num_constrs / 4;
    if (eta_limit < 50) eta_limit = 50;
    if (eta_limit > 100) eta_limit = 100;
    if (state->thresholds[5] > 0 && (int)state->thresholds[5] < eta_limit)
        eta_limit = (int)state->thresholds[5];

    if (outStall && state->basis &&
        state->eta_count >= eta_limit) {
        int n = state->num_vars;
        int m = state->num_constrs;

        int col_thresh = (int)(STALL_ALPHA * n + STALL_BETA);
        int row_thresh = (int)(STALL_BETA + STALL_ALPHA *
                               (m + state->bounds_propagated));

        if (state->cols_eliminated > col_thresh ||
            state->rows_eliminated > row_thresh) {
            *outStall = 1;
        } else {
            *outStall = 0;
        }

        /* Check 3: Objective stagnation at refactor boundary */
        double delta = state->obj_value - state->obj_at_last_refactor;
        if (delta < env->optimality_tol && delta != env->optimality_tol)
            state->iteration_mode = 1;

        state->obj_at_last_refactor = state->obj_value;

        /* Stall counters reset — refactorization handled by step.c Phase 9.
         * post_iterate should NOT refactorize independently, as the different
         * threshold (refactor_interval=50 vs adaptive eta_limit=100) causes
         * extra refactorization + xB recompute that alters the simplex path.
         * This was root cause of finnis 7.3% error (diagnose gets 0.2%). */
        state->rows_eliminated = 0;
        state->cols_eliminated = 0;
    }

    /* Check 2: Iteration limit */
    if (state->iteration >= state->max_iterations)
        return CXF_ITERATION_LIMIT;

    /* Check 4: User interrupt (simplex_iteration.md Check 4).
     * Check environment termination flag set by Ctrl+C handler or
     * cxf_terminate API call. */
    if (state->model_ref != NULL && state->model_ref->env != NULL) {
        CxfEnv *env = state->model_ref->env;
        if (env->terminate_flag ||
            (env->terminate_flag_ptr != NULL && *env->terminate_flag_ptr))
            return CXF_ITERATION_LIMIT;  /* Graceful termination */
    }

    return 0;
}

/*===========================================================================
 * cxf_simplex_phase_end — Phase-end processing (v2 P3.21)
 *
 * Called twice per iteration: pre-pivot and post-pivot.
 *
 * Phase 1: Constraint candidate processing
 *   - Free variables: check reduced costs for dual infeasibility
 *   - Basic constraints: remove inactive (slack > threshold)
 *   - doScan: remove small-contribution variables
 *
 * Phase 2: Activity bound recomputation for modified constraints
 *
 * Also handles Phase I → Phase II transition.
 *===========================================================================*/

int cxf_simplex_phase_end(SolverState *state, CxfEnv *env, int doScan) {
    if (state == NULL || env == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    BasisState *basis = state->basis;
    if (!basis) return CXF_OK;

    double opt_tol = env->optimality_tol;
    double feas_tol = env->feasibility_tol;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;

    /* Track modified constraints for selective activity recomputation.
     * Dynamically sized to num_constrs (was fixed 256, silently dropping
     * constraints beyond that limit — convexfeld-qxw8). */
    int *modified = (int *)malloc((size_t)m * sizeof(int));
    if (!modified) return CXF_ERROR_OUT_OF_MEMORY;
    int num_modified = 0;

    /*--- Phase 1: Constraint candidate processing ---*/

    /* 1a. Free variable dual feasibility check — Phase II only.
     * In Phase I, a free variable with nonzero RC has an IMPROVING direction
     * (the simplex should select it as entering variable). Declaring
     * INFEASIBLE during Phase I is wrong — it prevents finding feasibility.
     * Only in Phase II does a free-variable RC violation signal true dual
     * infeasibility (revised_simplex.md Step 8, two_phase_method.md §3). */
    if (state->phase != 1 && basis->var_status && state->work_dj)
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] != CXF_VAR_SUPERBASIC) continue;

        double rc = state->work_dj[j];
        if (fabs(rc) > opt_tol) {
            state->problem_var_index = j;
            free(modified);
            return CXF_INFEASIBLE;
        }
    }

    /* 1b. Inactive constraint removal — both phases.
     * two_phase_method.md Transition Step 4: remove constraints whose
     * activity bounds show they are not binding. Reduces effective
     * problem size. */
    if (state->min_activity && state->max_activity) {
        for (int i = 0; i < m; i++) {
            double min_a = state->min_activity[i];
            double max_a = state->max_activity[i];

            /* Skip infinite activities — can't determine slack */
            if (min_a <= -CXF_INFINITY || max_a >= CXF_INFINITY) continue;

            char sense = (state->work_sense) ? state->work_sense[i] : '<';
            double rhs = (state->work_rhs) ? state->work_rhs[i] : 0.0;
            double slack;

            /* Compute slack from activity bounds and RHS:
             *   <= : slack = rhs - max(a^T x). Positive when loose.
             *   >= : slack = min(a^T x) - rhs. Positive when loose.
             *   =  : always active, skip. */
            if (sense == '<' || sense == 'L')
                slack = rhs - max_a;
            else if (sense == '>' || sense == 'G')
                slack = min_a - rhs;
            else
                continue;  /* Equality constraints are always active */

            if (slack < 10.0 * feas_tol) continue;  /* Active constraint */

            /* This constraint has significant slack — mark for cleanup */
            int aux_var = n + i;
            if (aux_var < total && basis->var_status &&
                basis->var_status[aux_var] >= 0) {
                /* Auxiliary is basic — constraint is active, skip */
                continue;
            }

            /* Mark constraint variables as dirty for repricing */
            if (state->pricing && state->csr_row_ptr) {
                int64_t rs = state->csr_row_ptr[i];
                int64_t re = state->csr_row_ptr[i + 1];
                for (int64_t k = rs; k < re; k++) {
                    int col = state->csr_col_idx[k];
                    if (col >= 0 && col < n)
                        cxf_pricing_mark_dirty(state->pricing, col);
                }
            }

            modified[num_modified++] = i;

            /* Note: do NOT increment cols_eliminated here. Inactive
             * constraint identification is not column elimination —
             * it feeds stall detection thresholds in post_iterate. */
        }
    }

    /* 1c. Small-contribution variable scan (if doScan enabled) */
    if (doScan && state->csr_row_ptr &&
        state->work_ub && state->work_lb && state->work_x) {
        for (int i = 0; i < m; i++) {
            int64_t rs = state->csr_row_ptr[i];
            int64_t re = state->csr_row_ptr[i + 1];

            for (int64_t k = rs; k < re; k++) {
                int col = state->csr_col_idx[k];
                if (col < 0 || col >= n) continue;
                if (basis->var_status[col] >= 0) continue;

                double a = state->csr_values[k];
                double bound_range = state->work_ub[col] -
                                     state->work_lb[col];

                /* Variable has negligible contribution to constraint */
                if (fabs(a) * bound_range < feas_tol * 0.01) {
                    /* Fix at nearest bound */
                    double x = state->work_x[col];
                    double lb = state->work_lb[col];
                    double ub = state->work_ub[col];

                    if (fabs(x - lb) < fabs(x - ub))
                        basis->var_status[col] = CXF_VAR_AT_LOWER;
                    else if (ub < CXF_INFINITY)
                        basis->var_status[col] = CXF_VAR_AT_UPPER;

                    if (state->pricing)
                        cxf_pricing_mark_dirty(state->pricing, col);
                    state->cols_eliminated++;
                }
            }
        }
    }

    /*--- Phase 2: Activity bound recomputation for modified constraints ---*/
    if (num_modified > 0)
        cxf_compute_activity_bounds(state, num_modified, modified);

    /* Phase I → Phase II transition is handled by cxf_check_phase_one_end
     * in the orchestrator (solve_lp.c), not here. phase_end focuses on
     * constraint cleanup which only applies in Phase II. */

    free(modified);
    return CXF_OK;
}
