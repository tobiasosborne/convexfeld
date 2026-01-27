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
 * @brief Extract a sparse column into a dense array.
 *
 * @param matrix Sparse matrix
 * @param col Column index
 * @param dense Output dense array (must be size num_rows)
 */
static void extract_column(const SparseMatrix *matrix, int col, double *dense) {
    int num_rows = matrix->num_rows;

    /* Clear the dense array */
    memset(dense, 0, (size_t)num_rows * sizeof(double));

    /* Fill in nonzeros from CSC format */
    int64_t start = matrix->col_ptr[col];
    int64_t end = matrix->col_ptr[col + 1];

    for (int64_t k = start; k < end; k++) {
        int row = matrix->row_idx[k];
        dense[row] = matrix->values[k];
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

    if (basis == NULL || model == NULL || model->matrix == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = state->num_constrs;
    int n = state->num_vars;

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
     *=========================================================================*/
    if (state->pricing != NULL) {
        num_candidates = cxf_pricing_candidates(
            state->pricing,
            state->work_dj,       /* reduced costs */
            basis->var_status,    /* variable status */
            n,
            env->optimality_tol,
            candidates,
            10
        );
    } else {
        /* Fallback: scan all variables for most negative reduced cost */
        num_candidates = 0;
        double best_rc = -env->optimality_tol;
        for (int j = 0; j < n; j++) {
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
     *=========================================================================*/
    extract_column(model->matrix, entering, column);
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

    /* Step size based on ratio test */
    int leaving = basis->basic_vars[leavingRow];
    double x_leaving = state->work_x[leaving];
    double lb_leaving = state->work_lb[leaving];
    double ub_leaving = state->work_ub[leaving];

    if (pivotElement > 0) {
        stepSize = (ub_leaving - x_leaving) / pivotElement;
    } else {
        stepSize = (lb_leaving - x_leaving) / pivotElement;
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
     * Step 7: Update reduced costs (simple version)
     *=========================================================================*/
    /* For proper steepest edge, would need BTRAN and full update */
    /* Simplified: just set entering variable's reduced cost to 0 */
    state->work_dj[entering] = 0.0;

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
