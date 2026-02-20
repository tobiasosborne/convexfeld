/**
 * @file step.c
 * @brief Simplex iteration engine + pivot helper (v2 P3.20)
 *
 * cxf_simplex_step: Full iteration (pricing, FTRAN, ratio test, pivot, RC update)
 * cxf_apply_pivot:  Low-level pivot operation (primal update + eta vector)
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Iteration result codes */
#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* Refactorization threshold */
#define REFACTOR_INTERVAL  100

/* External function declarations */
extern int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                              const double *pivotCol, int enteringVar,
                              int leavingVar);
extern int cxf_pricing_candidates(PricingState *ctx, const double *reduced_costs,
                                  const int *var_status, int num_vars, double tolerance,
                                  int *candidates, int max_candidates);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);
extern int cxf_btran(BasisState *basis, int row, double *result);
extern int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                          const double *pivotColumn, int columnNZ,
                          int *leavingRow_out, double *pivotElement_out);
extern int cxf_solver_refactor(SolverState *ctx, CxfEnv *env);
extern void cxf_compute_reduced_costs(SolverState *state);

/**
 * @brief Execute the low-level pivot operation.
 *
 * Updates primal solution, entering variable value, creates eta vector,
 * and fixes leaving variable bound status. Called by cxf_simplex_step
 * after pricing and ratio test have determined entering/leaving variables.
 */
int cxf_apply_pivot(SolverState *state, int entering, int leavingRow,
                    const double *pivotCol, double stepSize) {
    int i, basicVar, leaving, result;

    if (state == NULL || pivotCol == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (state->basis == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    leaving = state->basis->basic_vars[leavingRow];

    /* Update basic variable values: x_B[i] -= stepSize * pivotCol[i] */
    int total_vars = state->num_vars + state->num_constrs;
    for (i = 0; i < state->num_constrs; i++) {
        basicVar = state->basis->basic_vars[i];
        if (basicVar < 0 || basicVar >= total_vars) continue;
        state->work_x[basicVar] -= stepSize * pivotCol[i];
    }

    /* Update entering variable value based on bound status */
    if (state->basis->var_status[entering] == -1) {
        state->work_x[entering] = state->work_lb[entering] + stepSize;
    } else {
        state->work_x[entering] = state->work_ub[entering] - stepSize;
    }

    /* Create eta vector and update basis state */
    result = cxf_pivot_with_eta(state->basis, leavingRow, pivotCol,
                                entering, leaving);

    /* Fix leaving variable status based on which bound it hit */
    if (result == CXF_OK && leaving >= 0 && leaving < total_vars) {
        double x_leave = state->work_x[leaving];
        double lb_leave = state->work_lb[leaving];
        double ub_leave = state->work_ub[leaving];
        double dist_to_lb = fabs(x_leave - lb_leave);
        double dist_to_ub = fabs(x_leave - ub_leave);
        if (dist_to_ub < dist_to_lb && ub_leave < CXF_INFINITY) {
            state->basis->var_status[leaving] = -2;
        }
    }

    return result;
}

/**
 * @brief Get coefficient for slack/surplus/artificial variable (fallback).
 */
static double get_auxiliary_coeff_fallback(const MatrixData *matrix, int row) {
    if (matrix == NULL || matrix->sense == NULL) return 1.0;
    char sense = matrix->sense[row];
    double rhs = (matrix->rhs != NULL) ? matrix->rhs[row] : 0.0;

    if (sense == '>' || sense == 'G') {
        return (rhs > 0) ? 1.0 : -1.0;
    }
    if (sense == '<' || sense == 'L') {
        return (rhs < 0) ? -1.0 : 1.0;
    }
    if (sense == '=') {
        return (rhs < 0) ? -1.0 : 1.0;
    }
    return 1.0;
}

/**
 * @brief Extract a column into a dense array.
 *
 * For original variables (col < n): extracts from sparse matrix.
 * For auxiliary variables (col >= n): generates identity column
 * with coefficient from basis->diag_coeff.
 */
static void extract_column_ext(const MatrixData *matrix, BasisState *basis,
                               int col, int n, int m, double *dense) {
    memset(dense, 0, (size_t)m * sizeof(double));

    if (col < n) {
        if (matrix == NULL) return;
        int64_t start = matrix->col_ptr[col];
        int64_t end = matrix->col_ptr[col + 1];
        for (int64_t k = start; k < end; k++) {
            int row = matrix->row_idx[k];
            dense[row] = matrix->values[k];
        }
    } else {
        int row = col - n;
        if (row >= 0 && row < m) {
            double coeff = (basis != NULL && basis->diag_coeff != NULL) ?
                basis->diag_coeff[row] :
                get_auxiliary_coeff_fallback(matrix, row);
            dense[row] = coeff;
        }
    }
}

/**
 * @brief Execute one step of the revised simplex method (v2 P3.20).
 *
 * Pricing, FTRAN, ratio test, step-size computation, BTRAN,
 * pivot (via cxf_apply_pivot), incremental RC update, refactorization.
 *
 * @param state Solver context
 * @param env Environment
 * @return ITERATE_CONTINUE (0), ITERATE_OPTIMAL (1),
 *         ITERATE_UNBOUNDED (3), or error code
 */
int cxf_simplex_step(SolverState *state, CxfEnv *env) {
    int rc;
    int entering = -1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int candidates[10];
    int num_candidates;

    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    BasisState *basis = state->basis;
    CxfModel *model = state->model_ref;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = state->num_constrs;
    int n = state->num_vars;

    /* Unconstrained LP: immediately optimal */
    if (m == 0) {
        state->iteration++;
        return ITERATE_OPTIMAL;
    }

    if (basis == NULL || model->matrix == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int total_vars = n + m;

    double *pivotCol = basis->work;
    if (pivotCol == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    double *column = state->work_column;
    if (column == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /*=====================================================================
     * Step 1: Pricing — select entering variable
     *=====================================================================*/
    if (state->use_bland) {
        num_candidates = 0;
        for (int j = 0; j < total_vars && num_candidates < 10; j++) {
            if (basis->var_status[j] >= 0) continue;
            double lb_j = state->work_lb[j];
            double ub_j = state->work_ub[j];
            if (ub_j <= lb_j + CXF_FEASIBILITY_TOL) continue;

            double rc_val = state->work_dj[j];
            if (basis->var_status[j] == -1 && rc_val < -env->optimality_tol) {
                candidates[num_candidates++] = j;
            } else if (basis->var_status[j] == -2 && rc_val > env->optimality_tol) {
                candidates[num_candidates++] = j;
            }
        }
    } else if (state->pricing != NULL) {
        num_candidates = cxf_pricing_candidates(
            state->pricing,
            state->work_dj,
            basis->var_status,
            total_vars,
            env->optimality_tol,
            candidates,
            10
        );
        /* Filter out FIXED variables */
        int new_count = 0;
        for (int k = 0; k < num_candidates; k++) {
            int j = candidates[k];
            double lb_j = state->work_lb[j];
            double ub_j = state->work_ub[j];
            if (ub_j > lb_j + CXF_FEASIBILITY_TOL) {
                candidates[new_count++] = j;
            }
        }
        num_candidates = new_count;
    } else {
        /* Fallback: most negative reduced cost */
        num_candidates = 0;
        double best_rc = -env->optimality_tol;
        for (int j = 0; j < total_vars; j++) {
            if (basis->var_status[j] >= 0) continue;

            double lb_j = state->work_lb[j];
            double ub_j = state->work_ub[j];
            if (ub_j <= lb_j + CXF_FEASIBILITY_TOL) continue;

            double rc_val = state->work_dj[j];
            if (basis->var_status[j] == -1 && rc_val < best_rc) {
                best_rc = rc_val;
                candidates[0] = j;
                num_candidates = 1;
            } else if (basis->var_status[j] == -2 && rc_val > env->optimality_tol) {
                if (-rc_val < best_rc) {
                    best_rc = -rc_val;
                    candidates[0] = j;
                    num_candidates = 1;
                }
            }
        }
    }

    if (num_candidates == 0) {
        return ITERATE_OPTIMAL;
    }

    /*=====================================================================
     * Steps 2-4: FTRAN, ratio test, step size — looped over candidates
     *=====================================================================*/
    int chosen_idx = 0;
    int last_candidate = num_candidates;

    for (int ci = 0; ci < last_candidate; ci++) {
        entering = candidates[ci];

        /* Step 2: FTRAN */
        extract_column_ext(model->matrix, basis, entering, n, m, column);
        rc = cxf_ftran(basis, column, pivotCol);
        if (rc != CXF_OK) return rc;

        /* Step 3: Ratio test */
        rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                            &leavingRow, &pivotElement);
        if (rc == CXF_UNBOUNDED) {
            if (state->use_bland && ci + 1 < last_candidate) continue;
            return ITERATE_UNBOUNDED;
        }
        if (rc != CXF_OK) return rc;

        /* Step 4: Compute step size */
        if (fabs(pivotElement) < CXF_PIVOT_TOL) {
            if (state->use_bland && ci + 1 < last_candidate) continue;
            return CXF_NUMERIC;
        }

        int leaving = basis->basic_vars[leavingRow];
        double x_leaving = state->work_x[leaving];
        double lb_leaving = state->work_lb[leaving];
        double ub_leaving = state->work_ub[leaving];

        if (pivotElement > 0) {
            stepSize = (x_leaving - lb_leaving) / pivotElement;
        } else {
            stepSize = (x_leaving - ub_leaving) / pivotElement;
        }
        if (stepSize < 0) stepSize = 0;

        /* Limit by entering variable's bound range */
        double x_entering = state->work_x[entering];
        double lb_entering = state->work_lb[entering];
        double ub_entering = state->work_ub[entering];
        if (basis->var_status[entering] == -1) {
            double max_step = ub_entering - x_entering;
            if (max_step < stepSize) stepSize = max_step;
        } else if (basis->var_status[entering] == -2) {
            double max_step = x_entering - lb_entering;
            if (max_step < stepSize) stepSize = max_step;
        }
        if (stepSize < 0) stepSize = 0;

        /* In Bland's mode, skip degenerate pivots if alternatives exist */
        if (state->use_bland && stepSize < 1e-8 && ci + 1 < last_candidate) {
            chosen_idx = ci;
            continue;
        }

        chosen_idx = ci;
        break;
    }

    /* If all candidates degenerate, re-run FTRAN/ratio for candidate[0] */
    if (chosen_idx > 0 && stepSize < 1e-8 && state->use_bland) {
        entering = candidates[0];
        extract_column_ext(model->matrix, basis, entering, n, m, column);
        cxf_ftran(basis, column, pivotCol);
        cxf_ratio_test(state, env, entering, pivotCol, m,
                        &leavingRow, &pivotElement);
        int leaving = basis->basic_vars[leavingRow];
        double x_leaving = state->work_x[leaving];
        double lb_leaving = state->work_lb[leaving];
        double ub_leaving = state->work_ub[leaving];
        if (pivotElement > 0) {
            stepSize = (x_leaving - lb_leaving) / pivotElement;
        } else {
            stepSize = (x_leaving - ub_leaving) / pivotElement;
        }
        if (stepSize < 0) stepSize = 0;
        double x_entering = state->work_x[entering];
        double lb_entering = state->work_lb[entering];
        double ub_entering = state->work_ub[entering];
        if (basis->var_status[entering] == -1) {
            double max_step = ub_entering - x_entering;
            if (max_step < stepSize) stepSize = max_step;
        } else if (basis->var_status[entering] == -2) {
            double max_step = x_entering - lb_entering;
            if (max_step < stepSize) stepSize = max_step;
        }
        if (stepSize < 0) stepSize = 0;
    }

    /* Cycling detection */
    if (stepSize < 1e-8) {
        state->degenerate_count++;
        if (!state->use_bland && state->degenerate_count > 50) {
            state->use_bland = 1;
        }
        if (state->use_bland && fabs(pivotElement) > CXF_PIVOT_TOL) {
            double scale = 1.0 + (state->degenerate_count / 100.0);
            stepSize = scale * CXF_FEASIBILITY_TOL / fabs(pivotElement);
        }
    } else {
        state->degenerate_count = 0;
    }

    /*=====================================================================
     * Step 5: BTRAN for leaving row (OLD basis, before pivot)
     *=====================================================================*/
    int leaving = basis->basic_vars[leavingRow];
    double d_entering = state->work_dj[entering];

    double *rho = state->work_cB;
    int btran_ok = (rho != NULL &&
                    cxf_btran(basis, leavingRow, rho) == CXF_OK);

    /*=====================================================================
     * Step 6: Pivot — update basis and solution
     *=====================================================================*/
    rc = cxf_apply_pivot(state, entering, leavingRow, pivotCol, stepSize);
    if (rc != CXF_OK) return rc;

    /*=====================================================================
     * Step 7: Update objective value
     *=====================================================================*/
    state->obj_value += d_entering * stepSize;

    /*=====================================================================
     * Step 8: Incremental reduced cost update
     *=====================================================================*/
    if (btran_ok) {
        double step_dual = d_entering / pivotElement;

        state->work_dj[entering] = 0.0;
        state->work_dj[leaving] = -step_dual;

        for (int j = 0; j < total_vars; j++) {
            if (j == entering || j == leaving) continue;
            if (basis->var_status[j] >= 0) continue;

            double rho_aj = 0.0;
            if (j < n && model->matrix != NULL) {
                int64_t start = model->matrix->col_ptr[j];
                int64_t end = model->matrix->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    rho_aj += rho[model->matrix->row_idx[k]]
                            * model->matrix->values[k];
                }
            } else if (j >= n) {
                int row = j - n;
                if (row >= 0 && row < m) {
                    double coeff = (basis->diag_coeff != NULL) ?
                        basis->diag_coeff[row] :
                        get_auxiliary_coeff_fallback(model->matrix, row);
                    rho_aj = rho[row] * coeff;
                }
            }

            state->work_dj[j] -= step_dual * rho_aj;
        }
    } else {
        cxf_compute_reduced_costs(state);
    }

    /*=====================================================================
     * Step 9: Refactorization + full RC recomputation
     *=====================================================================*/
    if (basis->pivots_since_refactor >= REFACTOR_INTERVAL) {
        cxf_solver_refactor(state, env);
        cxf_compute_reduced_costs(state);
    }

    state->iteration++;
    return ITERATE_CONTINUE;
}
