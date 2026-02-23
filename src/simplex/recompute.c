/**
 * @file recompute.c
 * @brief From-scratch recomputation of basic variables and objective.
 *
 * After LU refactorization, incrementally-maintained values drift due
 * to accumulated floating-point error. These functions recompute x_B
 * and obj from scratch, resetting error to single-factorization level.
 *
 * Spec: numerical_stability.md Section A
 *       revised_simplex.md Step 9.4 (x_B recomputation)
 *       numerical_stability.md line 47 (objective recomputation)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern int cxf_ftran(BasisState *basis, const double *column, double *result);

/**
 * @brief Recompute basic variable values from scratch.
 *
 * Solves x_B = B^{-1} (b - N x_N) using the current LU factorization,
 * replacing incrementally-maintained values that may have drifted.
 *
 * @param state SolverState with valid basis and working arrays.
 * @return CXF_OK on success, error code on failure.
 */
int cxf_recompute_xB(SolverState *state) {
    if (!state || !state->basis || !state->work_rhs)
        return CXF_ERROR_NULL_ARGUMENT;

    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    BasisState *basis = state->basis;

    double *rhs_adj = (double *)malloc((size_t)m * sizeof(double));
    double *xB = (double *)malloc((size_t)m * sizeof(double));
    if (!rhs_adj || !xB) {
        free(rhs_adj);
        free(xB);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Start with original RHS */
    memcpy(rhs_adj, state->work_rhs, (size_t)m * sizeof(double));

    /* Snap nonbasic variables to exact bound values, then subtract.
     * Nonbasic vars should be at their bounds by definition, but
     * incremental updates may have drifted them slightly. */
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] >= 0) continue;
        /* Snap to exact bound based on status */
        if (basis->var_status[j] == CXF_VAR_AT_LOWER &&
            state->work_lb[j] > -CXF_INFINITY)
            state->work_x[j] = state->work_lb[j];
        else if (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                 state->work_ub[j] < CXF_INFINITY)
            state->work_x[j] = state->work_ub[j];
        double xj = state->work_x[j];
        if (fabs(xj) < CXF_ZERO_TOL) continue;

        if (j < n && state->csc_col_ptr) {
            int64_t start = state->csc_col_ptr[j];
            int64_t end = state->csc_col_ptr[j + 1];
            for (int64_t k = start; k < end; k++)
                rhs_adj[state->csc_row_idx[k]] -= state->csc_values[k] * xj;
        } else if (j >= n && j < total) {
            int row = j - n;
            double coeff = (basis->diag_coeff) ?
                basis->diag_coeff[row] : 1.0;
            rhs_adj[row] -= coeff * xj;
        }
    }

    /* FTRAN: solve B * x_B = rhs_adj */
    int rc = cxf_ftran(basis, rhs_adj, xB);
    if (rc != CXF_OK) {
        free(rhs_adj);
        free(xB);
        return rc;
    }

    /* Update basic variable values */
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv >= 0 && bv < total)
            state->work_x[bv] = xB[i];
    }

    free(rhs_adj);
    free(xB);
    return CXF_OK;
}

/**
 * @brief Recompute objective value from scratch.
 *
 * Phase II: obj = sum(c_j * x_j) over all variables.
 * Phase I:  obj = sum of bound violations of basic variables.
 *
 * @param state SolverState with valid working arrays.
 */
void cxf_recompute_objective(SolverState *state) {
    if (!state) return;

    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    BasisState *basis = state->basis;

    if (state->phase == 1 && basis) {
        /* Reset all w-coefficients, then recompute from violations.
         * Must update work_obj[] so subsequent RC computation is valid. */
        for (int j = 0; j < total; j++)
            state->work_obj[j] = 0.0;
        double obj = 0.0;
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= total) continue;
            double xv = state->work_x[bv];
            double lb = state->work_lb[bv];
            double ub = state->work_ub[bv];
            if (xv < lb - CXF_FEASIBILITY_TOL) {
                state->work_obj[bv] = -1.0;
                obj += (lb - xv);
            } else if (xv > ub + CXF_FEASIBILITY_TOL) {
                state->work_obj[bv] = +1.0;
                obj += (xv - ub);
            }
        }
        state->obj_value = obj;
    } else {
        double obj = 0.0;
        for (int j = 0; j < total; j++)
            obj += state->work_obj[j] * state->work_x[j];
        state->obj_value = obj;
    }
}

/**
 * @brief Compute FTRAN residual ||a - B * (B^{-1} a)||_inf.
 *
 * Measures accuracy of the current basis inverse representation.
 * Used for residual monitoring per numerical_stability.md Section A.
 *
 * @param state SolverState with basis and CSC matrix.
 * @param a     Original column vector (pre-FTRAN input).
 * @param x     FTRAN result (B^{-1} a).
 * @return Max absolute residual, or 0.0 on error.
 */
double cxf_ftran_residual(SolverState *state, const double *a,
                          const double *x) {
    if (!state || !state->basis || !a || !x) return INFINITY;

    int m = state->num_constrs;
    int n = state->num_vars;
    BasisState *basis = state->basis;

    double *Bx = (double *)calloc((size_t)m, sizeof(double));
    if (!Bx) return INFINITY;

    /* Compute B * x = sum over basis columns k: col_k * x[k] */
    for (int k = 0; k < m; k++) {
        int bv = basis->basic_vars[k];
        double xk = x[k];
        if (fabs(xk) < CXF_ZERO_TOL) continue;

        if (bv < n && state->csc_col_ptr) {
            int64_t start = state->csc_col_ptr[bv];
            int64_t end = state->csc_col_ptr[bv + 1];
            for (int64_t p = start; p < end; p++)
                Bx[state->csc_row_idx[p]] += state->csc_values[p] * xk;
        } else if (bv >= n && bv < n + m) {
            int row = bv - n;
            double coeff = (basis->diag_coeff) ?
                basis->diag_coeff[row] : 1.0;
            Bx[row] += coeff * xk;
        }
    }

    double max_res = 0.0;
    for (int i = 0; i < m; i++) {
        double ri = fabs(a[i] - Bx[i]);
        if (ri > max_res) max_res = ri;
    }

    free(Bx);
    return max_res;
}
