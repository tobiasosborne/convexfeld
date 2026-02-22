/**
 * @file phase_one.c
 * @brief Phase I setup and Phase I to Phase II transition.
 *
 * Implements the implicit bound-violation Phase I approach per
 * two_phase_method.md spec (Maros 2003 Section 6.3). No explicit
 * artificial variables. Phase I objective = sum of bound violations,
 * encoded via dynamic w coefficients in work_obj.
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
extern int cxf_compute_reduced_costs(SolverState *state);
extern void cxf_pricing_invalidate(PricingState *ctx, int flags);
extern void cxf_pricing_set_level(PricingState *ctx, int level);

#define CXF_INVALID_ALL 0xFF

/**
 * @brief Set up Phase I using implicit bound-violation approach.
 *
 * All slacks/surpluses start as basic. Slack values can be negative
 * (infeasible). Phase I w coefficients: -1 if basic var below lb,
 * +1 if above ub, 0 if feasible. Phase I objective = sum of violations.
 */
int cxf_setup_phase_one(SolverState *state) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Step 1: Initialize structural variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        double lb = state->work_lb[j];
        if (lb <= -CXF_INFINITY) lb = 0.0;
        basis->var_status[j] = CXF_VAR_AT_LOWER;
        state->work_x[j] = lb;
    }

    /* Step 2: Set up slack/surplus variables as initial basis */
    for (int i = 0; i < m; i++) {
        int slack_idx = n + i;
        char sense = state->work_sense ? state->work_sense[i] : '<';

        /* Natural diag_coeff: ±1 per sense, scaled by D_r[i] if scaling active.
         * Row scaling changes slack column coefficients from ±1 to D_r*(±1). */
        double sense_sign = (sense == '>' || sense == 'G') ? -1.0 : 1.0;
        if (basis->diag_coeff != NULL) {
            basis->diag_coeff[i] = (state->row_scale != NULL)
                ? state->row_scale[i] * sense_sign : sense_sign;
        }

        /* Slack bounds */
        state->work_lb[slack_idx] = 0.0;
        state->work_ub[slack_idx] = CXF_INFINITY;
        if (sense == '=' || sense == 'E')
            state->work_ub[slack_idx] = 0.0;  /* equality: slack fixed at 0 */

        /* Make slack basic */
        basis->basic_vars[i] = slack_idx;
        basis->var_status[slack_idx] = i;

        /* Compute slack value: s = (rhs - a'x) / diag */
        double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
        double row_sum = 0.0;
        if (state->csc_col_ptr != NULL) {
            for (int j = 0; j < n; j++) {
                int64_t start = state->csc_col_ptr[j];
                int64_t end = state->csc_col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (state->csc_row_idx[k] == i) {
                        row_sum += state->csc_values[k] * state->work_x[j];
                        break;
                    }
                }
            }
        }
        state->work_x[slack_idx] = (rhs - row_sum) / basis->diag_coeff[i];
    }

    /* Step 2b: Apply MPS RANGES to slack bounds.
     * RANGES converts one-sided constraints to two-sided range constraints
     * by modifying the slack variable's bound range. */
    if (state->model_ref && state->model_ref->matrix &&
        state->model_ref->matrix->range_values) {
        double *rv = state->model_ref->matrix->range_values;
        for (int i = 0; i < m; i++) {
            double r = rv[i];
            if (r == 0.0) continue;
            int slack_idx = n + i;
            char sense = state->work_sense ? state->work_sense[i] : '<';
            if (sense == '<' || sense == 'L' ||
                sense == '>' || sense == 'G') {
                /* L/G: slack range is [0, |r|] */
                state->work_ub[slack_idx] = fabs(r);
            } else if (sense == '=' || sense == 'E') {
                /* E: slack range depends on sign of r */
                if (r > 0) {
                    state->work_lb[slack_idx] = -r;
                    state->work_ub[slack_idx] = 0.0;
                } else {
                    state->work_lb[slack_idx] = 0.0;
                    state->work_ub[slack_idx] = -r;
                }
            }
        }
    }

    /* Step 3: Use crash results to swap structurals into basis.
     * For rows marked BASIC_LOWER by crash, look for a singleton
     * structural that can replace the slack. */
    if (state->row_status != NULL && state->csc_col_ptr != NULL) {
        for (int i = 0; i < m; i++) {
            if (state->row_status[i] != CXF_ROW_BASIC_LOWER) continue;

            int slack_var = n + i;
            /* Only useful if slack is infeasible */
            double sv = state->work_x[slack_var];
            double sl = state->work_lb[slack_var];
            double su = state->work_ub[slack_var];
            if (sv >= sl - CXF_FEASIBILITY_TOL &&
                sv <= su + CXF_FEASIBILITY_TOL)
                continue;

            /* Find best singleton structural column in this row */
            int best_col = -1;
            double best_abs = 0.0;
            for (int j = 0; j < n; j++) {
                if (basis->var_status[j] >= 0) continue;
                if (state->col_nz_count == NULL) continue;
                if (state->col_nz_count[j] != 1) continue;
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
                basis->basic_vars[i] = best_col;
                basis->var_status[best_col] = i;
                basis->var_status[slack_var] = CXF_VAR_AT_LOWER;
                state->work_x[slack_var] = 0.0;

                /* Compute structural value from constraint */
                double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
                int64_t cs = state->csc_col_ptr[best_col];
                double aij = state->csc_values[cs];
                if (fabs(aij) > CXF_PIVOT_TOL)
                    state->work_x[best_col] = rhs / aij;
            }
        }
    }

    /* Step 4: Compute Phase I w coefficients and objective */
    int total = n + m;
    for (int j = 0; j < total; j++)
        state->work_obj[j] = 0.0;

    state->num_artificials = 0;  /* repurposed: count of infeasible BVs */
    state->obj_value = 0.0;

    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        double x = state->work_x[bv];
        double lb = state->work_lb[bv];
        double ub = state->work_ub[bv];
        if (x < lb - CXF_FEASIBILITY_TOL) {
            state->work_obj[bv] = -1.0;
            state->obj_value += (lb - x);
            state->num_artificials++;
        } else if (x > ub + CXF_FEASIBILITY_TOL) {
            state->work_obj[bv] = +1.0;
            state->obj_value += (x - ub);
            state->num_artificials++;
        }
    }

    /* Step 5: If already feasible, skip Phase I */
    if (state->num_artificials == 0) {
        /* Restore original objective for Phase II */
        if (state->model_ref && state->model_ref->obj_coeffs)
            memcpy(state->work_obj, state->model_ref->obj_coeffs,
                   (size_t)n * sizeof(double));
        for (int i = 0; i < m; i++)
            state->work_obj[n + i] = 0.0;
        state->phase = 2;
        return CXF_OK;
    }

    state->phase = 1;

#ifdef DEBUG_PHASE1
    fprintf(stderr, "[Phase I SETUP] infeasible_bvs=%d, initial_obj=%.6f\n",
            state->num_artificials, state->obj_value);
#endif

    return CXF_OK;
}

/**
 * @brief Transition from Phase I to Phase II.
 *
 * Restores original objective, fixes equality slacks at zero,
 * refactorizes, and recomputes objective/reduced costs.
 * No artificial pivot-out needed (implicit Phase I has no artificials).
 */
int cxf_transition_to_phase_two(SolverState *state, CxfModel *model) {
    int n = state->num_vars;
    int m = state->num_constrs;
    BasisState *basis = state->basis;

    /* Step 0: Unperturb if EXPAND bound widening was active in Phase I.
     * Restores saved bounds so Phase II starts with clean bounds. */
    if (state->perturb_expand_active) {
        extern int cxf_simplex_unperturb(SolverState *, CxfEnv *);
        cxf_simplex_unperturb(state, model->env);
    }

    /* Step 1: Restore original objective (re-scaled if scaling active) */
    for (int j = 0; j < n; j++) {
        state->work_obj[j] = model->obj_coeffs[j];
        if (state->col_scale != NULL)
            state->work_obj[j] *= state->col_scale[j];
    }
    for (int i = 0; i < m; i++) {
        state->work_obj[n + i] = 0.0;
        char sense = (state->work_sense != NULL) ? state->work_sense[i] : '<';
        if (sense == '=' || sense == 'E')
            state->work_ub[n + i] = 0.0;  /* fix equality slack at zero */
    }

    /* Step 2: Refactorize for Phase II accuracy */
    cxf_solver_refactor(state, model->env);

    /* Step 3: Recompute objective value with original objective.
     * Include slack range [n, n+m) — their obj=0, but algebraic completeness. */
    state->obj_value = 0.0;
    for (int j = 0; j < n + m; j++)
        state->obj_value += state->work_obj[j] * state->work_x[j];

    /* Step 3b: Recompute reduced costs for Phase II objective */
    cxf_compute_reduced_costs(state);

    state->phase = 2;
    state->use_bland = 0;
    state->degenerate_count = 0;

    /* Reset FIXED variables to AT_LOWER for Phase II pricing */
    int total = n + m;
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] == CXF_VAR_FIXED)
            basis->var_status[j] = CXF_VAR_AT_LOWER;
    }

    /* Reset pricing state at Phase I→II boundary */
    if (state->pricing) {
        cxf_pricing_invalidate(state->pricing, CXF_INVALID_ALL);
        cxf_pricing_set_level(state->pricing, 0);
    }

    return CXF_OK;
}
