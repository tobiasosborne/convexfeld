/**
 * @file pivot_update.c
 * @brief Incremental activity bound update after bound changes (F2 P3.19)
 *
 * Spec: docs/specs-v2/specs/modules/pivot_operations.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Incrementally update activity bounds when a variable bound changes.
 *
 * When variable j's bound changes by delta, update each constraint i where
 * a_ij != 0: adjust min/max activity by a_ij * delta (sign-dependent).
 *
 * @param state  Solver state with activity bounds and CSC matrix
 * @param var    Variable index whose bound changed
 * @param delta  Change in bound value (new - old)
 * @param is_lb  1 if lower bound changed, 0 if upper bound changed
 */
void cxf_pivot_update(SolverState *state, int var, double delta, int is_lb) {
    if (state == NULL || fabs(delta) < 1e-15) return;
    if (state->min_activity == NULL || state->max_activity == NULL) return;

    if (state->csc_col_ptr == NULL || var < 0 || var >= state->num_vars)
        return;

    int64_t start = state->csc_col_ptr[var];
    int64_t end = state->csc_col_ptr[var + 1];

    for (int64_t k = start; k < end; k++) {
        int row = state->csc_row_idx[k];
        double a = state->csc_values[k];

        if (is_lb) {
            /* Lower bound changed: affects min_activity if a>0, max if a<0 */
            if (a > 0) {
                state->min_activity[row] += a * delta;
            } else {
                state->max_activity[row] += a * delta;
            }
        } else {
            /* Upper bound changed: affects max_activity if a>0, min if a<0 */
            if (a > 0) {
                state->max_activity[row] += a * delta;
            } else {
                state->min_activity[row] += a * delta;
            }
        }
    }
}

/**
 * @brief Compute feasible step length bounds for ratio test (F2 P3.19).
 *
 * Given entering variable's pivot column, compute the max step before
 * any basic variable hits its bound.
 *
 * @param state      Solver state with bounds and basic vars
 * @param pivotCol   FTRAN result (dense, size m)
 * @param entering   Entering variable index
 * @param theta_max  Output: maximum feasible step
 * @return 0 on success, CXF_UNBOUNDED if no bound blocks
 */
int cxf_pivot_check(SolverState *state, const double *pivotCol,
                    int entering, double *theta_max) {
    if (state == NULL || pivotCol == NULL || theta_max == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = state->num_constrs;
    double best = CXF_INFINITY;
    int found = 0;

    for (int i = 0; i < m; i++) {
        if (fabs(pivotCol[i]) < CXF_PIVOT_TOL) continue;

        int bv = state->basis->basic_vars[i];
        double x_bv = state->work_x[bv];
        double lb_bv = state->work_lb[bv];
        double ub_bv = state->work_ub[bv];
        double theta;

        if (pivotCol[i] > 0) {
            theta = (x_bv - lb_bv) / pivotCol[i];
        } else {
            theta = (x_bv - ub_bv) / pivotCol[i];
        }

        if (theta < 0) theta = 0;
        if (theta < best) {
            best = theta;
            found = 1;
        }
    }

    if (!found) return CXF_UNBOUNDED;

    *theta_max = best;
    return 0;
}
