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
#include "convexfeld/cxf_pricing.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

extern int cxf_solver_refactor(SolverState *ctx, CxfEnv *env);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);
extern int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                              const double *pivotCol, int enteringVar,
                              int leavingVar);
extern void cxf_pricing_invalidate(PricingState *ctx, int flags);
extern void cxf_pricing_set_level(PricingState *ctx, int level);

/* From update.c */
#define CXF_INVALID_ALL 0xFF

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
        double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
        double row_sum = 0.0;
        if (state->csc_col_ptr != NULL) {
            for (int j = 0; j < n; j++) {
                double aij = 0.0;
                int64_t start = state->csc_col_ptr[j];
                int64_t end = state->csc_col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (state->csc_row_idx[k] == i) {
                        aij = state->csc_values[k]; break;
                    }
                }
                row_sum += aij * state->work_x[j];
            }
        }

        double slack_val = rhs - row_sum;
        char sense = state->work_sense ? state->work_sense[i] : '<';
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
        char sense = (state->work_sense != NULL) ? state->work_sense[i] : '<';
        if (sense == '=' || sense == 'E') {
            state->work_ub[var_idx] = 0.0;
        } else if ((sense == '>' || sense == 'G') &&
                   basis->diag_coeff != NULL && basis->diag_coeff[i] > 0.0) {
            basis->diag_coeff[i] = -1.0;
        }
    }
    /* P1.3: Force refactorization at Phase I→II transition. */
    cxf_solver_refactor(state, model->env);

    /* P1.4 (85dt): Pivot out zero-value artificial basic variables.
     * Artificials at zero are degenerate in the basis — they must be
     * replaced by structural or slack variables for Phase II correctness.
     * Spec: simplex_phases.md "Phase I to Phase II Transition". */
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv < n) continue;  /* structural variable — keep */
        int aux_row = bv - n;
        if (aux_row < 0 || aux_row >= m) continue;

        /* Only pivot out if artificial is at zero (degenerate) */
        if (fabs(state->work_x[bv]) > CXF_FEASIBILITY_TOL) continue;

        /* Find a nonbasic structural variable to enter this row.
         * Scan the row for a column with a nonzero coefficient. */
        int best_col = -1;
        double best_abs = 0.0;
        if (state->csc_col_ptr != NULL) {
            for (int j = 0; j < n; j++) {
                if (basis->var_status[j] >= 0) continue;  /* skip basic */
                int64_t cs = state->csc_col_ptr[j];
                int64_t ce = state->csc_col_ptr[j + 1];
                for (int64_t k = cs; k < ce; k++) {
                    if (state->csc_row_idx[k] == i &&
                        fabs(state->csc_values[k]) > best_abs) {
                        best_abs = fabs(state->csc_values[k]);
                        best_col = j;
                    }
                }
            }
        }
        if (best_col < 0 || best_abs < CXF_PIVOT_TOL) continue;

        /* FTRAN the entering column and do a degenerate pivot */
        double *col_buf = state->work_column;
        if (col_buf == NULL) continue;
        memset(col_buf, 0, (size_t)m * sizeof(double));
        int64_t cs = state->csc_col_ptr[best_col];
        int64_t ce = state->csc_col_ptr[best_col + 1];
        for (int64_t k = cs; k < ce; k++)
            col_buf[state->csc_row_idx[k]] = state->csc_values[k];

        double *ftran_buf = basis->work;
        if (ftran_buf == NULL) continue;
        if (cxf_ftran(basis, col_buf, ftran_buf) != CXF_OK) continue;
        if (fabs(ftran_buf[i]) < CXF_PIVOT_TOL) continue;

        /* Degenerate pivot: step size = 0, just swap basis */
        cxf_pivot_with_eta(basis, i, ftran_buf, best_col, bv);
        basis->var_status[bv] = CXF_VAR_AT_LOWER;
        state->work_x[bv] = 0.0;
    }

    /* Recompute objective value with original objective */
    state->obj_value = 0.0;
    for (int j = 0; j < n; j++)
        state->obj_value += state->work_obj[j] * state->work_x[j];

    state->phase = 2;
    state->use_bland = 0;
    state->degenerate_count = 0;

    /* Reset FIXED variables to AT_LOWER for Phase II pricing. */
    for (int j = 0; j < n + m; j++) {
        if (basis->var_status[j] == CXF_VAR_FIXED)
            basis->var_status[j] = CXF_VAR_AT_LOWER;
    }

    /* P1.5 (ho9l): Reset pricing state at Phase I→II boundary.
     * Candidate sets and tolerance levels from Phase I objective
     * are invalid for Phase II. */
    if (state->pricing) {
        cxf_pricing_invalidate(state->pricing, CXF_INVALID_ALL);
        cxf_pricing_set_level(state->pricing, 0);
    }

    return CXF_OK;
}
