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
 * @brief Initialize basis with original variables (no artificials).
 *
 * Attempts to find a feasible starting basis using simple heuristics.
 * Sets all variables at their lower bounds initially.
 *
 * @param state Solver context
 * @return CXF_OK on success
 */
static int init_simple_basis(SolverContext *state) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int n = state->num_vars;

    if (m > n) return CXF_ERROR_NOT_SUPPORTED;

    /* Initialize: all variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        basis->var_status[j] = -1;  /* At lower bound */
        state->work_x[j] = state->work_lb[j];
    }

    /* Make first m variables basic (simple crash basis) */
    for (int i = 0; i < m; i++) {
        basis->basic_vars[i] = i;
        basis->var_status[i] = i;  /* Basic in row i */
        if (state->model_ref->matrix && state->model_ref->matrix->rhs) {
            state->work_x[i] = state->model_ref->matrix->rhs[i];
        }
    }
    return CXF_OK;
}

/**
 * @brief Compute reduced costs for current objective.
 */
static void compute_reduced_costs(SolverContext *state) {
    int n = state->num_vars;
    BasisState *basis = state->basis;
    for (int j = 0; j < n; j++) {
        state->work_dj[j] = (basis->var_status[j] >= 0) ? 0.0 : state->work_obj[j];
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

    /* Set up initial basis */
    rc = init_simple_basis(state);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /* Compute initial reduced costs */
    compute_reduced_costs(state);
    state->phase = 2;
    int max_iter = state->max_iterations;

    /* Main iteration loop */
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
