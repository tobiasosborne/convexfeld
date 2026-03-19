/**
 * @file bfrt.c
 * @brief BFRT support — single-flip next-blocker search.
 *
 * T1.3 moved to single-flip BFRT inlined in ratio_test.c.
 * This file provides the next-blocker scan and ABI stubs for
 * the old multi-flip functions.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Find the next blocking row after a single BFRT flip.
 *
 * Scans all rows except skipRow for the minimum non-negative ratio
 * to any bound.  Handles both bounds per direction (Phase I safety).
 *
 * @return Row index of next blocker, or -1 if none.
 */
int cxf_bfrt_next_blocker(SolverState *state, const double *pivotCol,
                           int s, int skipRow, double feasTol,
                           double *out_ratio) {
    int bestRow = -1;
    double bestRatio = CXF_INFINITY;
    double bestPivot = 0.0;
    int m = state->num_constrs;
    int total = state->num_vars + m;
    const int *bv = state->basis->basic_vars;
    const double *wx = state->work_x;
    const double *wlb = state->work_lb;
    const double *wub = state->work_ub;

    for (int i = 0; i < m; i++) {
        if (i == skipRow) continue;
        double d_i = pivotCol[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;

        double sd = s * d_i;
        double r = CXF_INFINITY;
        if (sd > 0) {
            if (wlb[j] > -CXF_INFINITY) {
                double t = (wx[j] - wlb[j]) / sd;
                if (t >= 0 && t < r) r = t;
            }
            if (wub[j] < CXF_INFINITY && wx[j] > wub[j] + feasTol) {
                double t = (wx[j] - wub[j]) / sd;
                if (t >= 0 && t < r) r = t;
            }
        } else {
            if (wub[j] < CXF_INFINITY) {
                double t = (wub[j] - wx[j]) / (-sd);
                if (t >= 0 && t < r) r = t;
            }
            if (wlb[j] > -CXF_INFINITY && wx[j] < wlb[j] - feasTol) {
                double t = (wlb[j] - wx[j]) / (-sd);
                if (t >= 0 && t < r) r = t;
            }
        }
        if (r >= CXF_INFINITY || r < -feasTol) continue;

        if (r < bestRatio - feasTol ||
            (r <= bestRatio + feasTol && fabs(d_i) > bestPivot)) {
            bestRatio = r; bestRow = i; bestPivot = fabs(d_i);
        }
    }
    if (out_ratio) *out_ratio = bestRatio;
    return bestRow;
}

/* ABI stubs for old multi-flip functions (no-ops) */

void cxf_bfrt_extend_step(SolverState *state, const double *pivotColumn,
                           int s, int use_bland, double feasTol, double inf,
                           int *finalRow, double *theta,
                           int *leavingRow_out, double *pivotElement_out,
                           int *flip_rows_out, int max_flips,
                           int *num_flips_out) {
    (void)state; (void)pivotColumn; (void)s; (void)use_bland;
    (void)feasTol; (void)inf; (void)finalRow; (void)theta;
    (void)leavingRow_out; (void)pivotElement_out;
    (void)flip_rows_out; (void)max_flips;
    if (num_flips_out) *num_flips_out = 0;
}

void cxf_bfrt_set_status(int nflips, const int *flip_rows,
                          int finalRow, double theta, double feasTol,
                          int *status_out) {
    (void)nflips; (void)flip_rows; (void)finalRow;
    (void)theta; (void)feasTol;
    if (status_out && *status_out == 0)
        *status_out = CXF_RT_NORMAL_PIVOT;
}
