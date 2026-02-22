/**
 * @file scaling.c
 * @brief Column-only Ruiz equilibration for matrix scaling.
 *
 * Iteratively scales columns to unit infinity-norm, reducing
 * coefficient range and improving numerical stability.
 * Row scaling is omitted to avoid inconsistency with diag_coeff
 * (slack/surplus coefficients are ±1 and not in the CSC matrix).
 *
 * Scaling: A_s = A * D_c, c_s = D_c * c, lb_s = lb / D_c,
 *          ub_s = ub / D_c, y = D_c^{-1} * x
 * Unscale: x = D_c * y (primal), obj value invariant.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

#define RUIZ_MAX_ITERS  10
#define SCALE_MIN       1e-6
#define SCALE_MAX       1e6

/**
 * @brief Apply column-only Ruiz equilibration.
 *
 * Modifies csc_values, csr_values, work_obj, work_lb, work_ub in-place.
 * RHS and diag_coeff are unchanged (no row scaling).
 *
 * @param state SolverState with working copies of the constraint matrix.
 * @param row_scale Output: unused (set to 1.0 for compatibility).
 * @param col_scale Output: column scaling factors [n].
 */
void cxf_scale_problem(SolverState *state,
                        double *row_scale, double *col_scale) {
    int n = state->num_vars;
    int m = state->num_constrs;

    /* Initialize all scale factors to 1.0 (identity) */
    for (int i = 0; i < m; i++) row_scale[i] = 1.0;
    for (int j = 0; j < n; j++) col_scale[j] = 1.0;

    if (n == 0 || m == 0 || !state->csc_col_ptr || !state->csc_values)
        return;

    for (int iter = 0; iter < RUIZ_MAX_ITERS; iter++) {
        int converged = 1;

        /* Compute column infinity norms and scale */
        for (int j = 0; j < n; j++) {
            double max_abs = 0.0;
            int64_t s = state->csc_col_ptr[j];
            int64_t e = state->csc_col_ptr[j + 1];
            for (int64_t k = s; k < e; k++) {
                double a = fabs(state->csc_values[k]);
                if (a > max_abs) max_abs = a;
            }

            double c = 1.0;
            if (max_abs > 0.0) {
                c = 1.0 / sqrt(max_abs);
                if (c < SCALE_MIN) c = SCALE_MIN;
                if (c > SCALE_MAX) c = SCALE_MAX;
                if (fabs(c - 1.0) > 0.01) converged = 0;
            }

            /* Apply to CSC column */
            for (int64_t k = s; k < e; k++)
                state->csc_values[k] *= c;

            col_scale[j] *= c;
        }

        if (converged) break;
    }

    /* Apply to CSR if present */
    if (state->csr_row_ptr && state->csr_col_idx && state->csr_values) {
        for (int i = 0; i < m; i++) {
            int64_t s = state->csr_row_ptr[i];
            int64_t e = state->csr_row_ptr[i + 1];
            for (int64_t k = s; k < e; k++) {
                int j = state->csr_col_idx[k];
                if (j >= 0 && j < n)
                    state->csr_values[k] *= col_scale[j];
            }
        }
    }

    /* Scale objective: c_s[j] = c[j] * D_c[j] */
    for (int j = 0; j < n; j++)
        state->work_obj[j] *= col_scale[j];

    /* Scale variable bounds: lb_s = lb / D_c, ub_s = ub / D_c */
    for (int j = 0; j < n; j++) {
        double sc = col_scale[j];
        if (state->work_lb[j] > -CXF_INFINITY)
            state->work_lb[j] /= sc;
        if (state->work_ub[j] < CXF_INFINITY)
            state->work_ub[j] /= sc;
    }
}

/**
 * @brief Unscale primal solution after solving.
 *
 * Converts scaled solution y back to original: x[j] = D_c[j] * y[j].
 */
void cxf_unscale_solution(SolverState *state,
                          const double *col_scale) {
    int n = state->num_vars;
    for (int j = 0; j < n; j++)
        state->work_x[j] *= col_scale[j];
}
