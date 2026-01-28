/**
 * @file iterate.c
 * @brief Full cxf_simplex_iterate implementation (M7.1.2)
 *
 * Performs a single iteration of the simplex algorithm:
 * pricing, FTRAN, ratio test, and basis update.
 *
 * Spec: docs/specs/functions/simplex/cxf_simplex_iterate.md
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
extern int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
                                  const int *var_status, int num_vars, double tolerance,
                                  int *candidates, int max_candidates);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);
extern int cxf_ratio_test(SolverContext *state, CxfEnv *env, int enteringVar,
                          const double *pivotColumn, int columnNZ,
                          int *leavingRow_out, double *pivotElement_out);
extern int cxf_simplex_step(SolverContext *state, int entering, int leavingRow,
                            const double *pivotCol, double stepSize);
extern int cxf_basis_refactor(BasisState *basis);

/**
 * @brief Extract a column into a dense array.
 *
 * For original variables (col < n): extracts from sparse matrix.
 * For artificial variables (col >= n): generates identity column
 * with +1 in row (col - n).
 *
 * @param matrix Sparse matrix (may be NULL for artificial vars)
 * @param col Column index (0 to n+m-1)
 * @param n Number of original variables
 * @param m Number of constraints (rows)
 * @param dense Output dense array (must be size m)
 */
static void extract_column_ext(const SparseMatrix *matrix, int col, int n, int m,
                               double *dense) {
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
        /* Artificial variable: identity column */
        /* Artificial var col corresponds to constraint row (col - n) */
        int row = col - n;
        if (row >= 0 && row < m) {
            dense[row] = 1.0;
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
int cxf_simplex_iterate(SolverContext *state, CxfEnv *env) {
    int rc;
    int entering, leavingRow;
    double pivotElement, stepSize;
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

    /* Need temporary column array for extracting from matrix */
    double *column = (double *)calloc((size_t)m, sizeof(double));
    if (column == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /*=========================================================================
     * Step 1: Pricing - select entering variable
     * Scan all variables including artificials (indices n to n+m-1)
     *=========================================================================*/
    if (state->pricing != NULL) {
        num_candidates = cxf_pricing_candidates(
            state->pricing,
            state->work_dj,       /* reduced costs */
            basis->var_status,    /* variable status */
            total_vars,           /* Include artificial variables */
            env->optimality_tol,
            candidates,
            10
        );
    } else {
        /* Fallback: scan all variables for most negative reduced cost */
        num_candidates = 0;
        double best_rc = -env->optimality_tol;
        for (int j = 0; j < total_vars; j++) {
            if (basis->var_status[j] >= 0) {
                continue;  /* Skip basic variables */
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
        free(column);
        return ITERATE_OPTIMAL;  /* No improving variable found */
    }

    entering = candidates[0];  /* Take best candidate */

    /*=========================================================================
     * Step 2: FTRAN - compute pivot column B^(-1) * a_entering
     * For artificial vars (entering >= n), generates identity column
     *=========================================================================*/
    extract_column_ext(model->matrix, entering, n, m, column);
    rc = cxf_ftran(basis, column, pivotCol);
    if (rc != CXF_OK) {
        free(column);
        return rc;
    }

    /*=========================================================================
     * Step 3: Ratio test - select leaving variable
     *=========================================================================*/
    rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                        &leavingRow, &pivotElement);
    if (rc == CXF_UNBOUNDED) {
        free(column);
        return ITERATE_UNBOUNDED;
    }
    if (rc != CXF_OK) {
        free(column);
        return rc;
    }

    /*=========================================================================
     * Step 4: Compute step size
     *=========================================================================*/
    if (fabs(pivotElement) < CXF_PIVOT_TOL) {
        free(column);
        return CXF_NUMERIC;  /* Pivot too small */
    }

    /* Step size based on ratio test.
     * When entering var increases by stepSize, basic var changes by -stepSize * pivotElement.
     * - pivotElement > 0: basic var decreases toward lb
     * - pivotElement < 0: basic var increases toward ub
     */
    int leaving = basis->basic_vars[leavingRow];
    double x_leaving = state->work_x[leaving];
    double lb_leaving = state->work_lb[leaving];
    double ub_leaving = state->work_ub[leaving];

    if (pivotElement > 0) {
        /* Basic var decreases toward lower bound */
        stepSize = (x_leaving - lb_leaving) / pivotElement;
    } else {
        /* Basic var increases toward upper bound */
        stepSize = (x_leaving - ub_leaving) / pivotElement;
    }

    if (stepSize < 0) {
        stepSize = 0;  /* Degenerate pivot */
    }

    /*=========================================================================
     * Step 5: Pivot - update basis and solution
     *=========================================================================*/
    rc = cxf_simplex_step(state, entering, leavingRow, pivotCol, stepSize);
    if (rc != CXF_OK) {
        free(column);
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
        /* Build c_B vector (objective coeffs of basic variables) */
        double *cB = (double *)calloc((size_t)m, sizeof(double));
        if (cB != NULL) {
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
            free(cB);
        } else {
            /* Fallback if allocation fails */
            for (int i = 0; i < m; i++) {
                int basic_var = basis->basic_vars[i];
                if (basic_var >= 0 && basic_var < total_vars) {
                    state->work_pi[i] = state->work_obj[basic_var];
                } else {
                    state->work_pi[i] = 0.0;
                }
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
                    /* Artificial variable j corresponds to row (j - n) */
                    int row = j - n;
                    if (row >= 0 && row < m) {
                        dj -= state->work_pi[row] * 1.0;
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
        cxf_basis_refactor(basis);
    }

    state->iteration++;
    free(column);
    return ITERATE_CONTINUE;
}
