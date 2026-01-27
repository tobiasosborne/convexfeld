/**
 * @file solve_lp.c
 * @brief Main LP solver entry point (M7.1.4)
 *
 * Orchestrates the simplex solve sequence: initialization, basis setup,
 * iteration loop, and solution extraction. Implements two-phase method.
 *
 * Spec: docs/specs/functions/simplex/cxf_solve_lp.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Iteration result codes from iterate.c */
#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* External declarations */
extern int cxf_simplex_init(CxfModel *model, SolverContext **stateP);
extern void cxf_simplex_final(SolverContext *state);
extern int cxf_simplex_iterate(SolverContext *state, CxfEnv *env);
extern int cxf_extract_solution(SolverContext *state, CxfModel *model);

/**
 * @brief Set up Phase I with artificial variables.
 *
 * Creates initial basis using artificial variables:
 * - Original vars (0 to n-1): set at lower bounds, nonbasic
 * - Artificial vars (n to n+m-1): basic, values = slack needed for feasibility
 * - Phase I objective: minimize sum of artificials
 *
 * @param state Solver context
 * @return CXF_OK on success
 */
static int setup_phase_one(SolverContext *state) {
    BasisState *basis = state->basis;
    CxfModel *model = state->model_ref;
    SparseMatrix *mat = model->matrix;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Initialize: all original variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        double lb = state->work_lb[j];
        if (lb <= -CXF_INFINITY) lb = 0.0;  /* Free vars start at 0 */
        basis->var_status[j] = -1;  /* At lower bound */
        state->work_x[j] = lb;
    }

    /* Set up artificial variables as initial basis */
    state->num_artificials = m;
    for (int i = 0; i < m; i++) {
        int art_idx = n + i;  /* Artificial var for row i */

        /* Artificial is basic in row i */
        basis->basic_vars[i] = art_idx;
        basis->var_status[art_idx] = i;  /* Basic in row i */

        /* Compute artificial value = RHS - sum(a_ij * x_j) for original vars */
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        double row_sum = 0.0;

        /* Sum contributions from original variables at their bounds */
        for (int j = 0; j < n; j++) {
            /* Get coefficient a_ij */
            double aij = 0.0;
            int64_t start = mat->col_ptr[j];
            int64_t end = mat->col_ptr[j + 1];
            for (int64_t k = start; k < end; k++) {
                if (mat->row_idx[k] == i) {
                    aij = mat->values[k];
                    break;
                }
            }
            row_sum += aij * state->work_x[j];
        }

        /* Artificial value = slack needed */
        double art_val = rhs - row_sum;

        /* Handle constraint sense: for >= constraints, negate */
        char sense = mat->sense ? mat->sense[i] : '<';
        if (sense == '>' || sense == 'G') {
            art_val = -art_val;
        }

        /* Artificials must be non-negative; if negative, we have a problem */
        if (art_val < 0) art_val = fabs(art_val);

        state->work_x[art_idx] = art_val;

        /* Set artificial bounds and Phase I objective coefficient */
        state->work_lb[art_idx] = 0.0;
        state->work_ub[art_idx] = CXF_INFINITY;
        state->work_obj[art_idx] = 1.0;  /* Phase I: minimize sum of artificials */
    }

    /* Set original variables' Phase I objective to 0 */
    for (int j = 0; j < n; j++) {
        state->work_obj[j] = 0.0;
    }

    /* Compute initial Phase I objective = sum of artificials */
    state->obj_value = 0.0;
    for (int i = 0; i < m; i++) {
        state->obj_value += state->work_x[n + i];
    }

    state->phase = 1;
    return CXF_OK;
}

/**
 * @brief Transition from Phase I to Phase II.
 *
 * After Phase I finds a feasible basis (sum of artificials = 0):
 * - Restore original objective coefficients
 * - Set artificial objective coefficients to 0 (they should stay at zero)
 * - Recompute objective value with original coefficients
 *
 * @param state Solver context
 * @param model Original model with true objective
 * @return CXF_OK on success
 */
static int transition_to_phase_two(SolverContext *state, CxfModel *model) {
    int n = state->num_vars;
    int m = state->num_constrs;

    /* Restore original objective coefficients */
    for (int j = 0; j < n; j++) {
        state->work_obj[j] = model->obj_coeffs[j];
    }

    /* Set artificial objective coefficients to 0 (or large positive for big-M) */
    for (int i = 0; i < m; i++) {
        state->work_obj[n + i] = 0.0;
    }

    /* Recompute objective value with original objective */
    state->obj_value = 0.0;
    for (int j = 0; j < n; j++) {
        state->obj_value += state->work_obj[j] * state->work_x[j];
    }

    state->phase = 2;
    return CXF_OK;
}

/**
 * @brief Compute reduced costs: dj = cj - pi^T * Aj
 *
 * For correct simplex pricing, reduced costs must account for the
 * dual prices (shadow prices) from the current basis.
 *
 * @param state Solver context
 */
static void compute_reduced_costs(SolverContext *state) {
    CxfModel *model = state->model_ref;
    SparseMatrix *mat = model->matrix;
    BasisState *basis = state->basis;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total_vars = n + m;

    /* Step 1: Compute dual prices pi = cB * B^-1
     * For simplicity with identity-like basis (artificials), we approximate:
     * pi[i] = objective coefficient of basic variable in row i
     */
    for (int i = 0; i < m; i++) {
        int basic_var = basis->basic_vars[i];
        if (basic_var >= 0 && basic_var < total_vars) {
            state->work_pi[i] = state->work_obj[basic_var];
        } else {
            state->work_pi[i] = 0.0;
        }
    }

    /* Step 2: Compute reduced costs for all variables */
    for (int j = 0; j < total_vars; j++) {
        if (basis->var_status[j] >= 0) {
            /* Basic variable: reduced cost = 0 */
            state->work_dj[j] = 0.0;
        } else {
            /* Nonbasic variable: dj = cj - pi^T * Aj */
            double dj = state->work_obj[j];

            if (j < n && mat != NULL) {
                /* Original variable: subtract pi^T * column_j */
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    int row = mat->row_idx[k];
                    dj -= state->work_pi[row] * mat->values[k];
                }
            } else if (j >= n) {
                /* Artificial variable j corresponds to row (j - n) */
                /* Its column is identity: 1 in row (j-n), 0 elsewhere */
                int row = j - n;
                if (row >= 0 && row < m) {
                    dj -= state->work_pi[row] * 1.0;
                }
            }

            state->work_dj[j] = dj;
        }
    }
}

/**
 * @brief Solve unconstrained LP (no constraints).
 */
static int solve_unconstrained(CxfModel *model) {
    for (int j = 0; j < model->num_vars; j++) {
        if (model->lb[j] > model->ub[j] + CXF_FEASIBILITY_TOL) {
            model->status = CXF_INFEASIBLE;
            return CXF_INFEASIBLE;
        }
    }

    double obj_val = 0.0;
    for (int j = 0; j < model->num_vars; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];

        if (c < 0) {
            if (ub >= CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = ub;
            obj_val += c * ub;
        } else if (c > 0) {
            if (lb <= -CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = lb;
            obj_val += c * lb;
        } else {
            double val = (lb > 0.0) ? lb : ((ub < 0.0) ? ub : 0.0);
            if (model->solution) model->solution[j] = val;
        }
    }
    model->obj_val = obj_val;
    model->status = CXF_OPTIMAL;
    return CXF_OPTIMAL;
}

/**
 * @brief Get row coefficients as dense array.
 */
static void get_row_coeffs(SparseMatrix *mat, int row, int n, double *coeffs) {
    memset(coeffs, 0, (size_t)n * sizeof(double));
    for (int j = 0; j < n; j++) {
        int64_t start = mat->col_ptr[j];
        int64_t end = mat->col_ptr[j + 1];
        for (int64_t k = start; k < end; k++) {
            if (mat->row_idx[k] == row) { coeffs[j] = mat->values[k]; break; }
        }
    }
}

/**
 * @brief Check if two rows are parallel (same direction).
 */
static int rows_parallel(double *r1, double *r2, int n, double *scale) {
    double s = 0.0;
    int found = 0;
    for (int j = 0; j < n; j++) {
        if (fabs(r1[j]) < CXF_ZERO_TOL && fabs(r2[j]) < CXF_ZERO_TOL) continue;
        if (fabs(r1[j]) < CXF_ZERO_TOL || fabs(r2[j]) < CXF_ZERO_TOL) return 0;
        double ratio = r1[j] / r2[j];
        if (!found) { s = ratio; found = 1; }
        else if (fabs(ratio - s) > CXF_FEASIBILITY_TOL) return 0;
    }
    *scale = s;
    return found;
}

/**
 * @brief Check if problem is obviously infeasible via simple analysis.
 *
 * Two checks:
 * 1. Single constraint infeasibility (bound propagation)
 * 2. Parallel constraint contradiction (e.g., x+y<=1 and x+y>=3)
 */
static int check_obvious_infeasibility(CxfModel *model) {
    SparseMatrix *mat = model->matrix;
    if (mat == NULL) return 0;

    int m = mat->num_rows;
    int n = mat->num_cols;

    double *row1 = (double *)malloc((size_t)n * sizeof(double));
    double *row2 = (double *)malloc((size_t)n * sizeof(double));
    if (row1 == NULL || row2 == NULL) { free(row1); free(row2); return 0; }

    /* Check 1: Single constraint infeasibility via bound propagation */
    for (int i = 0; i < m; i++) {
        double row_min = 0.0, row_max = 0.0;
        get_row_coeffs(mat, i, n, row1);
        for (int j = 0; j < n; j++) {
            double aij = row1[j];
            if (aij == 0.0) continue;
            double lb = model->lb[j], ub = model->ub[j];
            if (aij > 0) {
                row_min += aij * lb;
                row_max += (ub >= CXF_INFINITY) ? CXF_INFINITY : aij * ub;
            } else {
                row_min += (ub >= CXF_INFINITY) ? -CXF_INFINITY : aij * ub;
                row_max += aij * lb;
            }
        }
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        char sense = mat->sense ? mat->sense[i] : '<';
        if ((sense == '<' || sense == 'L') && row_min > rhs + CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if ((sense == '>' || sense == 'G') && row_max < rhs - CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if (sense == '=') {
            if (row_min > rhs + CXF_FEASIBILITY_TOL || row_max < rhs - CXF_FEASIBILITY_TOL) {
                free(row1); free(row2); return 1;
            }
        }
    }

    /* Check 2: Parallel constraint contradiction */
    for (int i = 0; i < m; i++) {
        get_row_coeffs(mat, i, n, row1);
        double rhs1 = mat->rhs ? mat->rhs[i] : 0.0;
        char sense1 = mat->sense ? mat->sense[i] : '<';

        for (int j = i + 1; j < m; j++) {
            get_row_coeffs(mat, j, n, row2);
            double scale = 0.0;
            if (!rows_parallel(row1, row2, n, &scale)) continue;

            double rhs2 = mat->rhs ? mat->rhs[j] : 0.0;
            char sense2 = mat->sense ? mat->sense[j] : '<';

            /* Scale row2 to match row1: row2 * scale = row1 */
            double scaled_rhs2 = rhs2 * scale;
            char scaled_sense2 = sense2;
            if (scale < 0) {
                if (sense2 == '<') scaled_sense2 = '>';
                else if (sense2 == '>') scaled_sense2 = '<';
            }

            /* Now check: row1 sense1 rhs1 and row1 scaled_sense2 scaled_rhs2 */
            double lower = -CXF_INFINITY, upper = CXF_INFINITY;
            if (sense1 == '<' || sense1 == 'L') upper = fmin(upper, rhs1);
            else if (sense1 == '>' || sense1 == 'G') lower = fmax(lower, rhs1);
            else { lower = rhs1; upper = rhs1; }

            if (scaled_sense2 == '<' || scaled_sense2 == 'L') upper = fmin(upper, scaled_rhs2);
            else if (scaled_sense2 == '>' || scaled_sense2 == 'G') lower = fmax(lower, scaled_rhs2);
            else { lower = fmax(lower, scaled_rhs2); upper = fmin(upper, scaled_rhs2); }

            if (lower > upper + CXF_FEASIBILITY_TOL) {
                free(row1); free(row2); return 1;
            }
        }
    }

    free(row1); free(row2);
    return 0;
}

/**
 * @brief Check for obvious unboundedness via ray analysis.
 *
 * For each unbounded variable with improving objective direction,
 * check if constraints allow infinite increase.
 */
static int check_obvious_unboundedness(CxfModel *model) {
    SparseMatrix *mat = model->matrix;
    if (mat == NULL) return 0;

    int m = mat->num_rows;
    int n = mat->num_cols;

    /* For each variable with infinite bound in improving direction */
    for (int j = 0; j < n; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];

        /* Check if variable wants to go to +infinity (c < 0) */
        if (c < -CXF_FEASIBILITY_TOL && ub >= CXF_INFINITY) {
            /* Can we increase j without violating any constraint? */
            int can_increase = 1;
            for (int i = 0; i < m && can_increase; i++) {
                /* Get coefficient of variable j in constraint i */
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;

                char sense = mat->sense ? mat->sense[i] : '<';
                /* For <= constraint with positive coefficient, increasing j violates */
                if ((sense == '<' || sense == 'L') && aij > CXF_ZERO_TOL) {
                    can_increase = 0;
                }
                /* For >= constraint with negative coefficient, increasing j violates */
                if ((sense == '>' || sense == 'G') && aij < -CXF_ZERO_TOL) {
                    can_increase = 0;
                }
                /* For = constraint with any nonzero coefficient, can't go to infinity */
                if (sense == '=') {
                    can_increase = 0;
                }
            }
            if (can_increase) return 1;  /* Unbounded: can increase j to +inf */
        }

        /* Check if variable wants to go to -infinity (c > 0) */
        if (c > CXF_FEASIBILITY_TOL && lb <= -CXF_INFINITY) {
            int can_decrease = 1;
            for (int i = 0; i < m && can_decrease; i++) {
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;

                char sense = mat->sense ? mat->sense[i] : '<';
                if ((sense == '<' || sense == 'L') && aij < -CXF_ZERO_TOL) {
                    can_decrease = 0;
                }
                if ((sense == '>' || sense == 'G') && aij > CXF_ZERO_TOL) {
                    can_decrease = 0;
                }
                if (sense == '=') {
                    can_decrease = 0;
                }
            }
            if (can_decrease) return 1;  /* Unbounded: can decrease j to -inf */
        }
    }
    return 0;
}

/**
 * @brief Solve an LP using the simplex method.
 */
int cxf_solve_lp(CxfModel *model) {
    SolverContext *state = NULL;
    int rc, status;

    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }
    if (model->num_constrs == 0) return solve_unconstrained(model);

    CxfEnv *env = model->env;
    if (env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->matrix == NULL || model->matrix->col_ptr == NULL) {
        model->status = CXF_ERROR_NOT_SUPPORTED;
        return CXF_ERROR_NOT_SUPPORTED;
    }

    /* Check for obvious infeasibility via bound propagation */
    if (check_obvious_infeasibility(model)) {
        model->status = CXF_INFEASIBLE;
        return CXF_INFEASIBLE;
    }

    /* Check for obvious unboundedness via ray analysis */
    if (check_obvious_unboundedness(model)) {
        model->status = CXF_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* Initialize solver state */
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { model->status = rc; return rc; }

    int max_iter = state->max_iterations;

    /*=========================================================================
     * PHASE I: Find feasible basis using artificial variables
     *=========================================================================*/
    rc = setup_phase_one(state);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /* Compute initial Phase I reduced costs */
    compute_reduced_costs(state);

    /* Phase I iteration loop */
    while (state->iteration < max_iter) {
        status = cxf_simplex_iterate(state, env);

        if (status == ITERATE_OPTIMAL) {
            /* Phase I optimal - check if feasible */
            if (state->obj_value > env->feasibility_tol) {
                /* Sum of artificials > 0: no feasible solution */
                model->status = CXF_INFEASIBLE;
                cxf_simplex_final(state);
                return CXF_INFEASIBLE;
            }
            /* Feasible basis found - proceed to Phase II */
            break;
        } else if (status == ITERATE_UNBOUNDED) {
            /* Phase I unbounded is impossible (artificials have lb=0) - bug */
            model->status = CXF_ERROR_NOT_SUPPORTED;
            cxf_simplex_final(state);
            return CXF_ERROR_NOT_SUPPORTED;
        } else if (status < 0) {
            model->status = status;
            cxf_simplex_final(state);
            return status;
        }
    }

    if (state->iteration >= max_iter) {
        model->status = CXF_ITERATION_LIMIT;
        cxf_simplex_final(state);
        return CXF_ITERATION_LIMIT;
    }

    /*=========================================================================
     * PHASE II: Optimize original objective
     *=========================================================================*/
    rc = transition_to_phase_two(state, model);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /* Recompute reduced costs with original objective */
    compute_reduced_costs(state);

    /* Phase II iteration loop */
    while (state->iteration < max_iter) {
        status = cxf_simplex_iterate(state, env);

        if (status == ITERATE_OPTIMAL) {
            model->status = CXF_OPTIMAL;
            break;
        } else if (status == ITERATE_UNBOUNDED) {
            model->status = CXF_UNBOUNDED;
            break;
        } else if (status == ITERATE_INFEASIBLE) {
            model->status = CXF_INFEASIBLE;
            break;
        } else if (status < 0) {
            model->status = status;
            break;
        }
    }

    if (state->iteration >= max_iter) model->status = CXF_ITERATION_LIMIT;

    if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);

    cxf_simplex_final(state);
    return model->status;
}
