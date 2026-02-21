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

extern int cxf_fix_variables_at_bounds(BasisState *basis);
extern void cxf_compute_activity_bounds(SolverState *state, int count,
                                        const int *indices);
extern int cxf_compute_reduced_costs(SolverState *state);

#define STALL_ALPHA 0.5
#define STALL_BETA  3

/*===========================================================================
 * cxf_simplex_post_iterate — Post-iteration monitoring (v2 P3.20)
 *
 * Signature: (Model*, SolverState*, int *outStall) per v2 spec.
 * Currently takes (SolverState*, CxfEnv*, int *outStall).
 *
 * Checks: (1) stall detection, (2) iteration limit,
 *         (3) objective stagnation, (4) user interrupt.
 *===========================================================================*/

int cxf_simplex_post_iterate(SolverState *state, CxfEnv *env,
                             int *outStall) {
    if (state == NULL || env == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    if (state->work_counter)
        *state->work_counter += 1.0;

    /* Check 1: Stall detection — only at refactor boundaries */
    if (outStall && state->basis &&
        state->eta_count >= env->refactor_interval) {
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

        /* Trigger refactorization */
        int rc = cxf_fix_variables_at_bounds(state->basis);
        if (rc != CXF_OK) return rc;
        state->eta_count = 0;
        state->rows_eliminated = 0;
        state->cols_eliminated = 0;
    }

    /* Check 2: Iteration limit */
    if (state->iteration >= state->max_iterations)
        return CXF_ITERATION_LIMIT;

    /* Check 4: User interrupt (placeholder) */

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
    CxfModel *model = state->model_ref;
    if (!basis || !model) return CXF_OK;

    double opt_tol = env->optimality_tol;
    double feas_tol = env->feasibility_tol;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;

    /* Track modified constraints for selective activity recomputation */
    int modified[256];
    int num_modified = 0;

    /*--- Phase 1: Constraint candidate processing ---*/

    /* 1a. Free variable dual feasibility check — both phases.
     * P1.2 (fiyt): removed phase==2 guard. In Phase I, a free variable
     * with nonzero RC in the surrogate objective certifies infeasibility. */
    if (basis->var_status && state->work_dj)
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] != CXF_VAR_SUPERBASIC) continue;

        double rc = state->work_dj[j];
        if (fabs(rc) > opt_tol) {
            /* Free variable with significant reduced cost:
             * dual infeasibility — problem may be infeasible */
            state->problem_row_index = j;
            return CXF_INFEASIBLE;
        }
    }

    /* 1b. Inactive constraint removal — both phases.
     * P1.2 (fiyt): removed phase==2 guard per spec. */
    if (state->min_activity && state->max_activity) {
        for (int i = 0; i < m; i++) {
            /* Check if constraint is inactive: both activity bounds
             * are well within the constraint's slack range */
            double min_a = state->min_activity[i];
            double max_a = state->max_activity[i];

            /* Skip infinite activities */
            if (min_a <= -CXF_INFINITY || max_a >= CXF_INFINITY) continue;

            /* Inactive if max_activity is far from zero (much slack) */
            double slack = -max_a;  /* For <= : slack = rhs - max(sum) */
            if (slack < 10.0 * feas_tol) continue;  /* Active constraint */

            /* This constraint has significant slack — mark for cleanup */
            int aux_var = n + i;
            if (aux_var < total && basis->var_status &&
                basis->var_status[aux_var] >= 0) {
                /* Auxiliary is basic — constraint is active, skip */
                continue;
            }

            /* Mark constraint variables as dirty for repricing */
            if (state->pricing && model->matrix && model->matrix->row_ptr) {
                MatrixData *mat = model->matrix;
                int64_t rs = mat->row_ptr[i];
                int64_t re = mat->row_ptr[i + 1];
                for (int64_t k = rs; k < re; k++) {
                    int col = mat->col_idx[k];
                    if (col >= 0 && col < n)
                        cxf_pricing_mark_dirty(state->pricing, col);
                }
            }

            if (num_modified < 256)
                modified[num_modified++] = i;

            state->cols_eliminated++;
        }
    }

    /* 1c. Small-contribution variable scan (if doScan enabled) */
    if (doScan && model->matrix && model->matrix->row_ptr &&
        state->work_ub && state->work_lb && state->work_x) {
        MatrixData *mat = model->matrix;
        for (int i = 0; i < m && num_modified < 256; i++) {
            int64_t rs = mat->row_ptr[i];
            int64_t re = mat->row_ptr[i + 1];

            for (int64_t k = rs; k < re; k++) {
                int col = mat->col_idx[k];
                if (col < 0 || col >= n) continue;
                if (basis->var_status[col] >= 0) continue;

                double a = mat->row_values[k];
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

    return CXF_OK;
}
