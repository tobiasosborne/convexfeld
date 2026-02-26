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
#include <string.h>
#include <math.h>

#include "../basis/basis_internal.h"


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

    /* Use preallocated workspace to avoid per-call malloc.
     * work_column is safe: callers re-extract column after recompute.
     * work_cB is safe: not used until later in iteration. */
    double *rhs_adj = state->work_column;
    double *xB = state->work_cB;
    if (!rhs_adj || !xB)
        return CXF_ERROR_NULL_ARGUMENT;

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
    if (rc != CXF_OK)
        return rc;

    /* Update basic variable values */
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv >= 0 && bv < total)
            state->work_x[bv] = xB[i];
    }

    /* Iterative refinement (numerical_stability.md):
     * Compute residual r = (b - N*x_N) - B*x_B, solve B*dx = r,
     * then x_B += dx. One round typically recovers 2-4 digits.
     * Only run when infeasibility exceeds tolerance. */
    {
        /* Check if refinement is needed: scan for worst infeasibility */
        double worst = 0.0;
        for (int i2 = 0; i2 < m; i2++) {
            int bv2 = basis->basic_vars[i2];
            if (bv2 < 0 || bv2 >= total) continue;
            double x2 = state->work_x[bv2];
            if (x2 < state->work_lb[bv2])
                worst = fmax(worst, state->work_lb[bv2] - x2);
            else if (x2 > state->work_ub[bv2])
                worst = fmax(worst, x2 - state->work_ub[bv2]);
        }

        if (worst > 100.0 * CXF_FEASIBILITY_TOL) {
            /* Recompute residual r = (b - N*x_N) - B*x_B.
             * rhs_adj still holds (b - N*x_N) from above. But we
             * overwrote it during the first FTRAN. Rebuild it. */
            memcpy(rhs_adj, state->work_rhs, (size_t)m * sizeof(double));
            for (int j2 = 0; j2 < total; j2++) {
                if (basis->var_status[j2] >= 0) continue;
                double xj2 = state->work_x[j2];
                if (fabs(xj2) < CXF_ZERO_TOL) continue;
                if (j2 < n && state->csc_col_ptr) {
                    int64_t s2 = state->csc_col_ptr[j2];
                    int64_t e2 = state->csc_col_ptr[j2 + 1];
                    for (int64_t k2 = s2; k2 < e2; k2++)
                        rhs_adj[state->csc_row_idx[k2]] -=
                            state->csc_values[k2] * xj2;
                } else if (j2 >= n && j2 < total) {
                    int row2 = j2 - n;
                    double c2 = basis->diag_coeff ?
                        basis->diag_coeff[row2] : 1.0;
                    rhs_adj[row2] -= c2 * xj2;
                }
            }

            /* Subtract B*x_B to get residual */
            for (int i2 = 0; i2 < m; i2++) {
                int bv2 = basis->basic_vars[i2];
                if (bv2 < 0 || bv2 >= total) continue;
                double xb2 = state->work_x[bv2];
                if (fabs(xb2) < CXF_ZERO_TOL) continue;
                if (bv2 < n && state->csc_col_ptr) {
                    int64_t s2 = state->csc_col_ptr[bv2];
                    int64_t e2 = state->csc_col_ptr[bv2 + 1];
                    for (int64_t k2 = s2; k2 < e2; k2++)
                        rhs_adj[state->csc_row_idx[k2]] -=
                            state->csc_values[k2] * xb2;
                } else if (bv2 >= n && bv2 < total) {
                    int row2 = bv2 - n;
                    double c2 = basis->diag_coeff ?
                        basis->diag_coeff[row2] : 1.0;
                    rhs_adj[row2] -= c2 * xb2;
                }
            }

            /* Solve B*dx = residual */
            rc = cxf_ftran(basis, rhs_adj, xB);
            if (rc == CXF_OK) {
                /* Apply correction: x_B += dx */
                for (int i2 = 0; i2 < m; i2++) {
                    int bv2 = basis->basic_vars[i2];
                    if (bv2 >= 0 && bv2 < total)
                        state->work_x[bv2] += xB[i2];
                }
            }
        }
    }

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

    /* Use preallocated work_cB — safe because callers don't overlap */
    double *Bx = state->work_cB;
    if (!Bx) return INFINITY;
    memset(Bx, 0, (size_t)m * sizeof(double));

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

    return max_res;
}
