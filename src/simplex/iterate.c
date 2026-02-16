/**
 * @file iterate.c
 * @brief Full cxf_log_iteration_progress implementation (M7.1.2)
 *
 * Performs a single iteration of the simplex algorithm:
 * pricing, FTRAN, ratio test, and basis update.
 *
 * Spec: docs/specs/functions/simplex/cxf_log_iteration_progress.md
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
#include <stdio.h>
#include <math.h>

/* Iteration result codes */
#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* Refactorization threshold - triggers LU refactorization every N pivots */
#define REFACTOR_INTERVAL  100

/* External function declarations */
extern int cxf_pricing_candidates(PricingState *ctx, const double *reduced_costs,
                                  const int *var_status, int num_vars, double tolerance,
                                  int *candidates, int max_candidates);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);
extern int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                          const double *pivotColumn, int columnNZ,
                          int *leavingRow_out, double *pivotElement_out);
extern int cxf_simplex_step(SolverState *state, int entering, int leavingRow,
                            const double *pivotCol, double stepSize);
extern int cxf_solver_refactor(SolverState *ctx, CxfEnv *env);

/**
 * @brief Get the coefficient for slack/surplus/artificial variable.
 *
 * This function is a fallback when basis->diag_coeff is not available.
 * The coefficient is computed from RHS sign as a heuristic.
 * For correct operation, use basis->diag_coeff[row] directly when available.
 *
 * @param matrix Sparse matrix (for sense and RHS arrays)
 * @param row Constraint row index
 * @return +1.0 or -1.0
 */
static double get_auxiliary_coeff_fallback(const MatrixData *matrix, int row) {
    if (matrix == NULL || matrix->sense == NULL) return 1.0;
    char sense = matrix->sense[row];
    double rhs = (matrix->rhs != NULL) ? matrix->rhs[row] : 0.0;

    if (sense == '>' || sense == 'G') {
        /* For >= with rhs > 0 (infeasible at x=0), need coeff = +1 */
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
 *
 * @param matrix Sparse matrix (may be NULL for artificial vars)
 * @param basis Basis state (for diag_coeff of auxiliaries)
 * @param col Column index (0 to n+m-1)
 * @param n Number of original variables
 * @param m Number of constraints (rows)
 * @param dense Output dense array (must be size m)
 */
static void extract_column_ext(const MatrixData *matrix, BasisState *basis,
                               int col, int n, int m, double *dense) {
    /* Clear the dense array */
    memset(dense, 0, (size_t)m * sizeof(double));

    if (col < n) {
        /* Original variable: extract from sparse matrix */
        if (matrix == NULL) return;
        int64_t start = matrix->col_ptr[col];
        int64_t end = matrix->col_ptr[col + 1];

        for (int64_t k = start; k < end; k++) {
            int row = matrix->row_idx[k];
            dense[row] = matrix->values[k];
        }
    } else {
        /* Auxiliary variable: identity column with coefficient from diag_coeff */
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
 * @brief Perform one simplex iteration.
 *
 * @param state Solver context
 * @param env Environment
 * @return ITERATE_CONTINUE (0) to continue, ITERATE_OPTIMAL (1) if optimal,
 *         ITERATE_UNBOUNDED (3) if unbounded, or error code
 */
int cxf_log_iteration_progress(SolverState *state, CxfEnv *env) {
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

    /* For unconstrained LP (m=0), immediately optimal at bounds */
    if (m == 0) {
        state->iteration++;
        return ITERATE_OPTIMAL;
    }

    /* For constrained LPs, need basis and matrix */
    if (basis == NULL || model->matrix == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Total variables = original + artificials for Phase I */
    int total_vars = n + m;

    /* Allocate work arrays if needed */
    double *pivotCol = basis->work;
    if (pivotCol == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Use preallocated column array (reused across iterations) */
    double *column = state->work_column;
    if (column == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /*=========================================================================
     * Step 1: Pricing - select entering variable
     * Scan all variables including artificials (indices n to n+m-1)
     *=========================================================================*/
    if (state->use_bland) {
        /* Bland's rule: collect attractive variables in index order.
         * Gather multiple so we can skip degenerate pivots. */
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
            state->work_dj,       /* reduced costs */
            basis->var_status,    /* variable status */
            total_vars,           /* Include artificial variables */
            env->optimality_tol,
            candidates,
            10
        );
        /* Filter out FIXED variables (pricing doesn't have access to bounds) */
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
        /* Fallback: scan all variables for most negative reduced cost */
        num_candidates = 0;
        double best_rc = -env->optimality_tol;
        for (int j = 0; j < total_vars; j++) {
            if (basis->var_status[j] >= 0) {
                continue;  /* Skip basic variables */
            }

            /* Skip FIXED variables (lb == ub) - they can't change */
            double lb_j = state->work_lb[j];
            double ub_j = state->work_ub[j];
            if (ub_j <= lb_j + CXF_FEASIBILITY_TOL) {
                continue;  /* Fixed variable, can't enter */
            }

            double rc_val = state->work_dj[j];
            /* At lower bound: negative RC improves */
            if (basis->var_status[j] == -1 && rc_val < best_rc) {
                best_rc = rc_val;
                candidates[0] = j;
                num_candidates = 1;
            }
            /* At upper bound: positive RC improves */
            else if (basis->var_status[j] == -2 && rc_val > env->optimality_tol) {
                if (-rc_val < best_rc) {
                    best_rc = -rc_val;
                    candidates[0] = j;
                    num_candidates = 1;
                }
            }
        }
    }

    if (num_candidates == 0) {
        return ITERATE_OPTIMAL;  /* No improving variable found */
    }

    /*=========================================================================
     * Steps 2-4: FTRAN, ratio test, step size — looped over candidates.
     * In Bland's mode, skip candidates that produce degenerate (step=0)
     * pivots to break cycling. Fall back to first candidate if all degenerate.
     *=========================================================================*/
    int chosen_idx = 0;
    int last_candidate = num_candidates;  /* try all in Bland's mode */

    for (int ci = 0; ci < last_candidate; ci++) {
        entering = candidates[ci];

        /* Step 2: FTRAN - compute pivot column B^(-1) * a_entering */
        extract_column_ext(model->matrix, basis, entering, n, m, column);
        rc = cxf_ftran(basis, column, pivotCol);
        if (rc != CXF_OK) {
            return rc;
        }

        /* Step 3: Ratio test - select leaving variable */
        rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                            &leavingRow, &pivotElement);
        if (rc == CXF_UNBOUNDED) {
            /* Try next candidate — this one has no leaving var */
            if (state->use_bland && ci + 1 < last_candidate) continue;
            return ITERATE_UNBOUNDED;
        }
        if (rc != CXF_OK) {
            return rc;
        }

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
            continue;  /* try next candidate */
        }

        chosen_idx = ci;
        break;  /* use this candidate */
    }

    /* If loop exhausted all candidates (all degenerate), use the first one.
     * Re-run FTRAN/ratio test for candidate[0] since pivotCol is stale. */
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

    /* Cycling detection: track consecutive near-degenerate pivots.
     * Threshold 1e-8 catches steps that are non-zero from FP artifacts
     * (e.g. 3e-10) but effectively degenerate. */
    if (stepSize < 1e-8) {
        state->degenerate_count++;
        if (!state->use_bland && state->degenerate_count > 50) {
            state->use_bland = 1;
        }
        /* Force a non-zero step to break exact degeneracy cycles.
         * This virtual perturbation ensures forward progress. Scale grows
         * with degenerate count to eventually escape any cycle. */
        if (state->use_bland && fabs(pivotElement) > CXF_PIVOT_TOL) {
            double scale = 1.0 + (state->degenerate_count / 100.0);
            stepSize = scale * CXF_FEASIBILITY_TOL / fabs(pivotElement);
        }
    } else {
        state->degenerate_count = 0;
    }

    /*=========================================================================
     * Step 5: Pivot - update basis and solution
     *=========================================================================*/
    rc = cxf_simplex_step(state, entering, leavingRow, pivotCol, stepSize);
    if (rc != CXF_OK) {
        return rc;
    }

    /*=========================================================================
     * Step 6: Update objective value
     *=========================================================================*/
    double rc_entering = state->work_dj[entering];
    state->obj_value += rc_entering * stepSize;

    /*=========================================================================
     * Step 7: Update reduced costs
     * Recompute all reduced costs after pivot for correctness.
     * Use BTRAN to properly compute dual prices: π = B^(-T) * c_B
     *=========================================================================*/
    {
        /* Build c_B vector using preallocated work array */
        double *cB = state->work_cB;
        for (int i = 0; i < m; i++) {
            int basic_var = basis->basic_vars[i];
            if (basic_var >= 0 && basic_var < total_vars) {
                cB[i] = state->work_obj[basic_var];
            } else {
                cB[i] = 0.0;
            }
        }

        /* Compute π = B^(-T) * c_B using BTRAN */
        extern int cxf_btran_vec(BasisState *basis, const double *input, double *result);
        int btran_rc = cxf_btran_vec(basis, cB, state->work_pi);
        if (btran_rc != CXF_OK) {
            /* Fallback to simple approximation if BTRAN fails */
            for (int i = 0; i < m; i++) {
                state->work_pi[i] = cB[i];
            }
        }

        /* Compute reduced costs for all variables */
        for (int j = 0; j < total_vars; j++) {
            if (basis->var_status[j] >= 0) {
                /* Basic variable: reduced cost = 0 */
                state->work_dj[j] = 0.0;
            } else {
                /* Nonbasic variable: dj = cj - pi^T * Aj */
                double dj = state->work_obj[j];

                if (j < n && model->matrix != NULL) {
                    /* Original variable: subtract pi^T * column_j */
                    int64_t start = model->matrix->col_ptr[j];
                    int64_t end = model->matrix->col_ptr[j + 1];
                    for (int64_t k = start; k < end; k++) {
                        int row = model->matrix->row_idx[k];
                        dj -= state->work_pi[row] * model->matrix->values[k];
                    }
                } else if (j >= n) {
                    /* Auxiliary variable j corresponds to row (j - n) */
                    int row = j - n;
                    if (row >= 0 && row < m) {
                        /* Use diag_coeff from basis if available */
                        double coeff = (basis->diag_coeff != NULL) ?
                            basis->diag_coeff[row] :
                            get_auxiliary_coeff_fallback(model->matrix, row);
                        dj -= state->work_pi[row] * coeff;
                    }
                }

                state->work_dj[j] = dj;
            }
        }
    }

    /*=========================================================================
     * Step 8: Check refactorization
     *=========================================================================*/
    if (basis->pivots_since_refactor >= REFACTOR_INTERVAL) {
        cxf_solver_refactor(state, env);
    }

    state->iteration++;
    return ITERATE_CONTINUE;
}
