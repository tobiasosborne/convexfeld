/**
 * @file phase_one.c
 * @brief Phase I setup and Phase I to Phase II transition.
 *
 * Extracted from solve_lp.c. Sets up artificial variables for Phase I
 * and transitions to Phase II when a feasible basis is found.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <stdio.h>

extern int cxf_solver_refactor(SolverState *ctx, CxfEnv *env);

/**
 * @brief Set up Phase I with slack/artificial variables.
 *
 * Creates initial basis using slack and artificial variables:
 * - Original vars (0 to n-1): set at lower bounds, nonbasic
 * - For <= constraints: slack variable (obj coeff = 0)
 * - For >= constraints: surplus + artificial if needed
 * - For = constraints: artificial variable (obj coeff = 1)
 * - Phase I objective: minimize sum of artificial variables
 */
int cxf_setup_phase_one(SolverState *state) {
    BasisState *basis = state->basis;
    CxfModel *model = state->model_ref;
    MatrixData *mat = model->matrix;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Initialize: all original variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        double lb = state->work_lb[j];
        if (lb <= -CXF_INFINITY) lb = 0.0;
        basis->var_status[j] = -1;
        state->work_x[j] = lb;
    }

    state->num_artificials = 0;

    for (int i = 0; i < m; i++) {
        int var_idx = n + i;
        basis->basic_vars[i] = var_idx;
        basis->var_status[var_idx] = i;

        /* Compute slack value = RHS - sum(a_ij * x_j) */
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        double row_sum = 0.0;
        for (int j = 0; j < n; j++) {
            double aij = 0.0;
            int64_t start = mat->col_ptr[j];
            int64_t end = mat->col_ptr[j + 1];
            for (int64_t k = start; k < end; k++) {
                if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
            }
            row_sum += aij * state->work_x[j];
        }

        double slack_val = rhs - row_sum;
        char sense = mat->sense ? mat->sense[i] : '<';
        state->work_lb[var_idx] = 0.0;
        state->work_ub[var_idx] = CXF_INFINITY;
        double diag = 1.0;

        if (sense == '<' || sense == 'L') {
            if (slack_val >= 0) {
                diag = 1.0;
                state->work_x[var_idx] = slack_val;
                state->work_obj[var_idx] = 0.0;
            } else {
                diag = -1.0;
                state->work_x[var_idx] = -slack_val;
                state->work_obj[var_idx] = 1.0;
                state->num_artificials++;
            }
        } else if (sense == '>' || sense == 'G') {
            double surplus_val = row_sum - rhs;
            if (surplus_val >= 0) {
                diag = -1.0;
                state->work_x[var_idx] = surplus_val;
                state->work_obj[var_idx] = 0.0;
            } else {
                diag = 1.0;
                state->work_x[var_idx] = -surplus_val;
                state->work_obj[var_idx] = 1.0;
                state->num_artificials++;
            }
        } else {
            if (slack_val >= 0) {
                diag = 1.0;
                state->work_x[var_idx] = slack_val;
            } else {
                diag = -1.0;
                state->work_x[var_idx] = -slack_val;
            }
            state->work_obj[var_idx] = 1.0;
            if (fabs(slack_val) > CXF_FEASIBILITY_TOL)
                state->num_artificials++;
        }

        if (basis->diag_coeff != NULL)
            basis->diag_coeff[i] = diag;
    }

    for (int j = 0; j < n; j++)
        state->work_obj[j] = 0.0;

    /* Compute initial Phase I objective = sum of artificial values */
    state->obj_value = 0.0;
    for (int i = 0; i < m; i++) {
        int var_idx = n + i;
        if (state->work_obj[var_idx] > 0.5)
            state->obj_value += state->work_x[var_idx];
    }

    state->phase = 1;

#ifdef DEBUG_PHASE1
    fprintf(stderr, "[Phase I SETUP] num_artificials=%d, initial_obj=%.6f\n",
            state->num_artificials, state->obj_value);
#endif

    return CXF_OK;
}

/**
 * @brief Transition from Phase I to Phase II.
 *
 * Restores original objective, fixes artificial variables at zero
 * for equality constraints, and recomputes objective value.
 */
int cxf_transition_to_phase_two(SolverState *state, CxfModel *model) {
    int n = state->num_vars;
    int m = state->num_constrs;
    MatrixData *mat = model->matrix;

    /* Restore original objective coefficients */
    for (int j = 0; j < n; j++)
        state->work_obj[j] = model->obj_coeffs[j];

    /* Set auxiliary objective to 0; fix artificials for = constraints;
     * flip >= artificials to surplus (diag +1 → -1) so Phase II
     * maintains the >= direction instead of allowing violation. */
    BasisState *basis = state->basis;
    for (int i = 0; i < m; i++) {
        int var_idx = n + i;
        state->work_obj[var_idx] = 0.0;
        char sense = (mat != NULL && mat->sense != NULL) ? mat->sense[i] : '<';
        if (sense == '=' || sense == 'E') {
            state->work_ub[var_idx] = 0.0;
        } else if ((sense == '>' || sense == 'G') &&
                   basis->diag_coeff != NULL && basis->diag_coeff[i] > 0.0) {
            basis->diag_coeff[i] = -1.0;
        }
    }
    /* P1.3 (vopd): Force refactorization at Phase I→II transition.
     * Spec numerical_stability.md §A.4 lists phase transition as an
     * explicit refactorization trigger. Accumulated Phase I error is
     * reset by a fresh LU of the current basis. */
    cxf_solver_refactor(state, model->env);

    /* Recompute objective value with original objective */
    state->obj_value = 0.0;
    for (int j = 0; j < n; j++)
        state->obj_value += state->work_obj[j] * state->work_x[j];

    state->phase = 2;
    state->use_bland = 0;
    state->degenerate_count = 0;

    /* Reset FIXED variables to AT_LOWER for Phase II pricing.
     * EXPAND perturbation (P2.6) may have marked nonbasic variables as
     * FIXED during Phase I stalling. These must be eligible for Phase II. */
    for (int j = 0; j < n + m; j++) {
        if (basis->var_status[j] == CXF_VAR_FIXED) {
            basis->var_status[j] = CXF_VAR_AT_LOWER;
        }
    }

    return CXF_OK;
}
