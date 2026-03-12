/**
 * @file scaling.c
 * @brief Full row+column Ruiz equilibration for matrix scaling.
 *
 * Implements Strategy 3 from matrix_finalization.md (Ruiz 2001):
 * iteratively scales rows and columns to unit infinity-norm.
 *
 * Scaling transforms:
 *   A_s[i,j] = D_r[i] * A[i,j] * D_c[j]
 *   b_s[i]   = D_r[i] * b[i]
 *   diag_s[i]= D_r[i] * diag_orig[i]   (slack column coefficients)
 *   c_s[j]   = D_c[j] * c[j]           (j < n only)
 *   lb_s[j]  = lb[j] / D_c[j]          (j < n only)
 *   ub_s[j]  = ub[j] / D_c[j]          (j < n only)
 *
 * Unscale: x[j] = D_c[j] * y[j] for structural vars.
 * Slack values and objective value are invariant.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define RUIZ_MAX_ITERS  10
#define SCALE_MIN       1e-6
#define SCALE_MAX       1e6
#define CONVERGE_TOL    0.01
/* NOTE: Scaling is effectively DISABLED. RANGE_THRESHOLD=1e30 means
 * needs_scaling() almost never triggers. This is intentional — scaling
 * requires multi-candidate pricing support before it can be enabled.
 * Do NOT lower this threshold without testing all Netlib instances. */
#define RANGE_THRESHOLD 1e30

/**
 * @brief Check if matrix needs scaling (Phase 1 validation).
 *
 * Returns 1 if the coefficient range exceeds RANGE_THRESHOLD.
 */
static int needs_scaling(const SolverState *state) {
    if (!state->csc_col_ptr || !state->csc_values) return 0;
    int n = state->num_vars;
    double global_max = 0.0, global_min = CXF_INFINITY;
    for (int j = 0; j < n; j++) {
        int64_t s = state->csc_col_ptr[j];
        int64_t e = state->csc_col_ptr[j + 1];
        for (int64_t k = s; k < e; k++) {
            double a = fabs(state->csc_values[k]);
            if (a > CXF_ZERO_TOL) {
                if (a > global_max) global_max = a;
                if (a < global_min) global_min = a;
            }
        }
    }
    if (global_min < CXF_ZERO_TOL) return 1;
    return (global_max / global_min > RANGE_THRESHOLD);
}

/**
 * @brief Apply full row+column Ruiz equilibration.
 *
 * Modifies csc_values, csr_values, work_rhs, work_obj,
 * work_lb, work_ub, and diag_coeff in-place.
 *
 * @param state SolverState with working copies.
 * @param row_scale Output: row scaling factors [m].
 * @param col_scale Output: column scaling factors [n].
 */
void cxf_scale_problem(SolverState *state,
                        double *row_scale, double *col_scale) {
    int n = state->num_vars;
    int m = state->num_constrs;

    for (int i = 0; i < m; i++) row_scale[i] = 1.0;
    for (int j = 0; j < n; j++) col_scale[j] = 1.0;

    if (n == 0 || m == 0 || !state->csc_col_ptr || !state->csc_values)
        return;
    if (!needs_scaling(state)) return;

    /* Allocate row norm scratch buffer */
    double *r_norm = (double *)calloc((size_t)m, sizeof(double));
    if (!r_norm) return;

    for (int iter = 0; iter < RUIZ_MAX_ITERS; iter++) {
        int converged = 1;

        /* Row phase: compute row infinity norms from CSC + slack.
         * Include the implicit slack column coefficient (diag_coeff)
         * to prevent over-scaling rows where slack dominates. */
        for (int i = 0; i < m; i++) {
            /* Start with slack column contribution */
            r_norm[i] = (state->basis && state->basis->diag_coeff)
                ? fabs(state->basis->diag_coeff[i]) : 1.0;
        }
        for (int j = 0; j < n; j++) {
            int64_t s = state->csc_col_ptr[j];
            int64_t e = state->csc_col_ptr[j + 1];
            for (int64_t k = s; k < e; k++) {
                int row = state->csc_row_idx[k];
                double a = fabs(state->csc_values[k]);
                if (a > r_norm[row]) r_norm[row] = a;
            }
        }

        /* Row phase: scale */
        for (int i = 0; i < m; i++) {
            double f = 1.0;
            if (r_norm[i] > 0.0) {
                f = 1.0 / sqrt(r_norm[i]);
                if (f < SCALE_MIN) f = SCALE_MIN;
                if (f > SCALE_MAX) f = SCALE_MAX;
                if (fabs(f - 1.0) > CONVERGE_TOL) converged = 0;
            }
            r_norm[i] = f;  /* reuse buffer as per-iteration factor */
        }

        /* Apply row factors to CSC */
        for (int j = 0; j < n; j++) {
            int64_t s = state->csc_col_ptr[j];
            int64_t e = state->csc_col_ptr[j + 1];
            for (int64_t k = s; k < e; k++)
                state->csc_values[k] *= r_norm[state->csc_row_idx[k]];
        }

        /* Accumulate row scale */
        for (int i = 0; i < m; i++)
            row_scale[i] *= r_norm[i];

        /* Column phase: compute column norms and scale */
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
                if (fabs(c - 1.0) > CONVERGE_TOL) converged = 0;
            }
            for (int64_t k = s; k < e; k++)
                state->csc_values[k] *= c;
            col_scale[j] *= c;
        }

#ifdef DEBUG_SCALING
        fprintf(stderr, "[RUIZ] iter=%d converged=%d\n", iter, converged);
#endif
        if (converged) break;
    }

    free(r_norm);

    /* Apply cumulative row+col scaling to CSR if present */
    if (state->csr_row_ptr && state->csr_col_idx && state->csr_values) {
        for (int i = 0; i < m; i++) {
            int64_t s = state->csr_row_ptr[i];
            int64_t e = state->csr_row_ptr[i + 1];
            for (int64_t k = s; k < e; k++) {
                int j = state->csr_col_idx[k];
                if (j >= 0 && j < n)
                    state->csr_values[k] *= row_scale[i] * col_scale[j];
            }
        }
    }

    /* Scale RHS: b_s[i] = D_r[i] * b[i] */
    if (state->work_rhs) {
        for (int i = 0; i < m; i++)
            state->work_rhs[i] *= row_scale[i];
    }

    /* Scale diag_coeff: D_r[i] * (±1)
     * NOTE: At this point diag_coeff is 1.0 (from basis_state init).
     * Phase I setup will overwrite with row_scale[i] * sense_sign.
     * So this multiplication is redundant but harmless. */
    if (state->basis && state->basis->diag_coeff) {
        for (int i = 0; i < m; i++)
            state->basis->diag_coeff[i] *= row_scale[i];
    }

    /* Scale objective: c_s[j] = D_c[j] * c[j] (structural only) */
    for (int j = 0; j < n; j++)
        state->work_obj[j] *= col_scale[j];

    /* Scale structural bounds: lb_s = lb/D_c, ub_s = ub/D_c */
    for (int j = 0; j < n; j++) {
        double sc = col_scale[j];
        if (state->work_lb[j] > -CXF_INFINITY)
            state->work_lb[j] /= sc;
        if (state->work_ub[j] < CXF_INFINITY)
            state->work_ub[j] /= sc;
    }
    /* Slack bounds are NOT scaled — slack values are invariant */

#ifdef DEBUG_SCALING
    {
        double rmin = 1e30, rmax = 0, cmin = 1e30, cmax = 0;
        for (int i = 0; i < m; i++) {
            if (row_scale[i] < rmin) rmin = row_scale[i];
            if (row_scale[i] > rmax) rmax = row_scale[i];
        }
        for (int j = 0; j < n; j++) {
            if (col_scale[j] < cmin) cmin = col_scale[j];
            if (col_scale[j] > cmax) cmax = col_scale[j];
        }
        fprintf(stderr, "[SCALING] row=[%.3e,%.3e] col=[%.3e,%.3e]\n",
                rmin, rmax, cmin, cmax);
        /* Post-scaling coefficient range */
        double gmax2 = 0, gmin2 = 1e30;
        for (int j2 = 0; j2 < n; j2++) {
            int64_t s2 = state->csc_col_ptr[j2];
            int64_t e2 = state->csc_col_ptr[j2 + 1];
            for (int64_t k2 = s2; k2 < e2; k2++) {
                double a2 = fabs(state->csc_values[k2]);
                if (a2 > 1e-15) {
                    if (a2 > gmax2) gmax2 = a2;
                    if (a2 < gmin2) gmin2 = a2;
                }
            }
        }
        fprintf(stderr, "[SCALING] post-scale coeff range: [%.3e, %.3e] ratio=%.1f\n",
                gmin2, gmax2, gmax2/gmin2);
    }
#endif
}

/**
 * @brief Unscale primal solution after solving.
 *
 * x[j] = D_c[j] * y[j] for structural variables only.
 */
void cxf_unscale_solution(SolverState *state,
                          const double *col_scale) {
    if (!col_scale) return;
    int n = state->num_vars;
    for (int j = 0; j < n; j++)
        state->work_x[j] *= col_scale[j];
}
