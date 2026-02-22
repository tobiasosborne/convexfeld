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
        int slack_idx = n + i;       /* slack/surplus variable */
        int art_idx = n + m + i;     /* artificial variable */

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

        /* NATURAL diag_coeff — never changes between phases */
        double diag = (sense == '>' || sense == 'G') ? -1.0 : 1.0;
        if (basis->diag_coeff != NULL)
            basis->diag_coeff[i] = diag;

        /* Default: both slack and artificial at lb=0 */
        state->work_lb[slack_idx] = 0.0;
        state->work_ub[slack_idx] = CXF_INFINITY;
        state->work_lb[art_idx] = 0.0;
        state->work_ub[art_idx] = CXF_INFINITY;

        /* Determine if constraint needs an artificial */
        int needs_art = 0;
        if (sense == '<' || sense == 'L') {
            needs_art = (slack_val < -CXF_FEASIBILITY_TOL);
        } else if (sense == '>' || sense == 'G') {
            needs_art = (slack_val > CXF_FEASIBILITY_TOL);
            /* For >=: surplus = -slack_val. Violated when slack_val > 0. */
        } else { /* = */
            needs_art = (fabs(slack_val) > CXF_FEASIBILITY_TOL);
        }

        /* Artificial column coefficient: sign chosen so value is positive.
         * art_coeff[i] = +1 if slack_val >= 0, -1 if slack_val < 0.
         * With B[i,i] = art_coeff[i], x_B[i] = slack_val / art_coeff[i] = |slack_val|. */
        double art_sign = (slack_val >= 0) ? 1.0 : -1.0;
        if (state->art_coeff != NULL)
            state->art_coeff[i] = art_sign;

        if (!needs_art) {
            /* Constraint satisfied: slack/surplus is basic */
            basis->basic_vars[i] = slack_idx;
            basis->var_status[slack_idx] = i;
            basis->var_status[art_idx] = CXF_VAR_AT_LOWER;

            /* Basic value = slack_val / diag (from B^{-1} * b) */
            state->work_x[slack_idx] = (sense == '>' || sense == 'G')
                ? -slack_val   /* surplus = row_sum - rhs = -slack_val */
                : slack_val;
            state->work_obj[slack_idx] = 0.0;
            state->work_x[art_idx] = 0.0;
            state->work_obj[art_idx] = 0.0;
        } else {
            /* Constraint violated: artificial is basic, slack nonbasic at 0 */
            basis->basic_vars[i] = art_idx;
            basis->var_status[art_idx] = i;
            basis->var_status[slack_idx] = CXF_VAR_AT_LOWER;

            state->work_x[art_idx] = fabs(slack_val);
            state->work_obj[art_idx] = 1.0;
            state->work_x[slack_idx] = 0.0;
            state->work_obj[slack_idx] = 0.0;
            state->num_artificials++;
        }
    }

    /* Zero original objective for Phase I */
    for (int j = 0; j < n; j++)
        state->work_obj[j] = 0.0;

    /* P5.3: Use crash results to select structural variables into basis.
     * For rows marked BASIC_LOWER by crash, look for a singleton structural
     * column that can replace the slack/artificial, reducing Phase I work.
     * Only swap if the structural variable's value is feasible. */
    if (state->row_status != NULL && state->csc_col_ptr != NULL) {
        for (int i = 0; i < m; i++) {
            if (state->row_status[i] != CXF_ROW_BASIC_LOWER) continue;

            /* Only useful if this row has an artificial */
            int art_var = n + m + i;
            if (state->work_obj[art_var] < 0.5) continue;

            /* Find best singleton structural column in this row */
            int best_col = -1;
            double best_abs = 0.0;
            for (int j = 0; j < n; j++) {
                if (basis->var_status[j] >= 0) continue;  /* already basic */
                if (state->col_nz_count == NULL) continue;
                if (state->col_nz_count[j] != 1) continue; /* not singleton */

                /* Check if column j has a nonzero in row i */
                int64_t cs = state->csc_col_ptr[j];
                int64_t ce = state->csc_col_ptr[j + 1];
                for (int64_t k = cs; k < ce; k++) {
                    if (state->csc_row_idx[k] == i) {
                        double aij = fabs(state->csc_values[k]);
                        if (aij > CXF_PIVOT_TOL && aij > best_abs) {
                            best_abs = aij;
                            best_col = j;
                        }
                        break;
                    }
                }
            }

            if (best_col >= 0) {
                /* Swap: put structural var in basis, make artificial nonbasic */
                basis->basic_vars[i] = best_col;
                basis->var_status[best_col] = i;
                basis->var_status[art_var] = CXF_VAR_AT_LOWER;

                /* Set artificial to zero (nonbasic at lower) */
                state->work_x[art_var] = 0.0;
                state->work_obj[art_var] = 0.0;
                state->num_artificials--;

                /* Compute structural var value from constraint */
                double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
                int64_t cs = state->csc_col_ptr[best_col];
                double aij = state->csc_values[cs];
                if (fabs(aij) > CXF_PIVOT_TOL)
                    state->work_x[best_col] = rhs / aij;
            }
        }
    }

    /* Compute initial Phase I objective = sum of artificial values */
    state->obj_value = 0.0;
    for (int i = 0; i < m; i++) {
        int art_idx = n + m + i;
        if (state->work_obj[art_idx] > 0.5)
            state->obj_value += state->work_x[art_idx];
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

    /* Zero all auxiliary objectives and fix artificials for Phase II.
     * diag_coeff is already natural (never flipped) — no restoration needed.
     * Artificials at [n+m, n+2m) get fixed at lb=ub=0.
     * Equality constraint slacks at [n+i] also fixed at lb=ub=0. */
    BasisState *basis = state->basis;
    for (int i = 0; i < m; i++) {
        char sense = (state->work_sense != NULL) ? state->work_sense[i] : '<';
        state->work_obj[n + i] = 0.0;       /* slack/surplus obj = 0 */
        state->work_obj[n + m + i] = 0.0;   /* artificial obj = 0 */
        state->work_ub[n + m + i] = 0.0;    /* fix artificial at zero */
        if (sense == '=' || sense == 'E')
            state->work_ub[n + i] = 0.0;    /* fix equality slack at zero */
    }
    /* P1.3: Force refactorization at Phase I→II transition. */
    cxf_solver_refactor(state, model->env);

    /* P1.4 (85dt): Pivot out zero-value artificial basic variables.
     * Artificials at zero are degenerate in the basis — they must be
     * replaced by structural or slack variables for Phase II correctness.
     * Spec: simplex_phases.md "Phase I to Phase II Transition". */
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv < n + m) continue;  /* structural or slack/surplus — keep */
        /* bv is an artificial at [n+m, n+2m). Map to constraint row. */
        int art_row = bv - n - m;
        if (art_row < 0 || art_row >= m) continue;

        /* Only pivot out if artificial is at zero (degenerate) */
        if (fabs(state->work_x[bv]) > CXF_FEASIBILITY_TOL) continue;

        /* Find a nonbasic variable (structural or slack) to replace artificial.
         * First try the slack/surplus for this constraint row. */
        int best_col = -1;
        double best_abs = 0.0;
        int slack_var = n + art_row;
        if (basis->var_status[slack_var] < 0) {
            /* Slack is nonbasic — it has coefficient diag_coeff at this row */
            best_col = slack_var;
            best_abs = fabs(basis->diag_coeff[art_row]);
        }
        /* Also scan structural columns for better pivot */
        if (state->csc_col_ptr != NULL) {
            for (int j = 0; j < n; j++) {
                if (basis->var_status[j] >= 0) continue;
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
        if (best_col < n) {
            /* Structural: extract from CSC */
            int64_t cs = state->csc_col_ptr[best_col];
            int64_t ce = state->csc_col_ptr[best_col + 1];
            for (int64_t k = cs; k < ce; k++)
                col_buf[state->csc_row_idx[k]] = state->csc_values[k];
        } else {
            /* Slack/surplus: singleton column */
            int row = best_col - n;
            col_buf[row] = basis->diag_coeff[row];
        }

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
    for (int j = 0; j < n + 2 * m; j++) {
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
