/**
 * @file ratio_test.c
 * @brief Harris two-pass ratio test for simplex pivot selection.
 *
 * Implements cxf_ratio_test per harris_ratio_test.md (Maros 2003 Ch. 8).
 *
 * Pass 1: find theta_max = min { (slack + band) / |d_i| } over all rows.
 * Pass 2: among strict ratios <= theta_max, select largest pivot (or
 *          smallest index under Bland's rule).
 *
 * Stage 3 (BFRT bound flips) is in bfrt.c.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include "simplex_internal.h"
#include <math.h>

/**
 * @brief Compute Harris ratio for one basic variable against its bounds.
 *
 * Returns the minimum ratio to any active bound.  When band > 0 the
 * feasibility window is widened (Harris relaxation, Pass 1); band = 0
 * gives the strict ratio (Pass 2).
 */
static double row_ratio(double x, double lb, double ub, double sd,
                        double band, double feasTol, double inf) {
    double r, ratio = inf;
    if (sd > 0) {
        if (lb > -inf) {
            r = (x - lb + band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
        if (ub < inf && x > ub + feasTol) {
            r = (x - ub + band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
    } else {
        if (ub < inf) {
            r = (x - ub - band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
        if (lb > -inf && x < lb - feasTol) {
            r = (x - lb - band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
    }
    return ratio;
}

/** @brief Compute initial step length theta for the Stage 2 blocker. */
static double compute_theta(int lv, int total, int s, double feasTol,
                             double inf, const double *pivotColumn,
                             int finalRow, const double *wx,
                             const double *wlb, const double *wub) {
    double theta = inf;
    double sd = s * pivotColumn[finalRow];
    if (lv >= 0 && lv < total) {
        if (sd > 0 && wlb[lv] > -inf) {
            double r = (wx[lv] - wlb[lv]) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
        if (sd > 0 && wub[lv] < inf && wx[lv] > wub[lv] + feasTol) {
            double r = (wx[lv] - wub[lv]) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
        if (sd < 0 && wub[lv] < inf) {
            double r = (wx[lv] - wub[lv]) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
        if (sd < 0 && wlb[lv] > -inf && wx[lv] < wlb[lv] - feasTol) {
            double r = (wx[lv] - wlb[lv]) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
    }
    if (theta >= inf) theta = 0;
    if (theta < 0) theta = 0;
    return theta;
}

int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out,
                   int *status_out, double *theta_out,
                   int *flip_rows_out, int max_flips,
                   int *num_flips_out) {
    (void)columnNZ;
    if (!state || !env || !pivotColumn || !leavingRow_out || !pivotElement_out)
        return CXF_ERROR_NULL_ARGUMENT;

    const double feasTol = env->feasibility_tol;
    const double inf     = env->infinity;
    const double band    = feasTol;           /* V2 harris_ratio_test.md */
    const int m     = state->num_constrs;
    const int total = state->num_vars + m;
    const int *bv   = state->basis->basic_vars;
    const double *wx  = state->work_x;
    const double *wlb = state->work_lb;
    const double *wub = state->work_ub;

    /* Entering direction */
    int s = 1;
    if (state->basis && enteringVar >= 0) {
        int vs = state->basis->var_status[enteringVar];
        if (vs == CXF_VAR_AT_UPPER)
            s = -1;
        else if (vs == CXF_VAR_SUPERBASIC && state->work_dj &&
                 state->work_dj[enteringVar] > 0.0)
            s = -1;
    }

    /* V2: pre-check bound validity */
    for (int i = 0; i < m; i++) {
        if (fabs(pivotColumn[i]) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;
        if (wlb[j] > wub[j] + feasTol)
            return CXF_INFEASIBLE;
    }

    /* --- Pass 1: theta_max = min relaxed ratio --- */
    double minRatio = inf;
    int minRow = -1;
    for (int i = 0; i < m; i++) {
        double d_i = pivotColumn[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;
        double ratio = row_ratio(wx[j], wlb[j], wub[j],
                                 s * d_i, band, feasTol, inf);
        if (ratio < minRatio) { minRatio = ratio; minRow = i; }
    }
    if (minRow == -1) {
        if (status_out) *status_out = CXF_RT_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* --- Pass 2: best pivot among strict ratios <= theta_max --- */
    double maxPivot = fabs(pivotColumn[minRow]);
    int finalRow = minRow;
    int blandBest = bv[minRow];
    for (int i = 0; i < m; i++) {
        double d_i = pivotColumn[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;
        double ratio = row_ratio(wx[j], wlb[j], wub[j],
                                 s * d_i, 0.0, feasTol, inf);
        if (ratio > minRatio) continue;
        if (state->use_bland) {
            if (j < blandBest) { blandBest = j; finalRow = i; }
        } else {
            if (fabs(d_i) > maxPivot) { maxPivot = fabs(d_i); finalRow = i; }
        }
    }

    *leavingRow_out  = finalRow;
    *pivotElement_out = pivotColumn[finalRow];

    /* Compute initial theta for Stage 2 blocker */
    double theta = compute_theta(bv[finalRow], total, s, feasTol, inf,
                                 pivotColumn, finalRow, wx, wlb, wub);

    /* --- Stage 3: BFRT (bfrt.c) --- */
    int nflips = 0;
    cxf_bfrt_extend_step(state, pivotColumn, s, state->use_bland, feasTol,
                          inf, &finalRow, &theta,
                          leavingRow_out, pivotElement_out,
                          flip_rows_out, max_flips, &nflips);
    if (num_flips_out) *num_flips_out = nflips;
    if (theta_out) *theta_out = theta;
    cxf_bfrt_set_status(nflips, flip_rows_out, finalRow, theta, feasTol,
                         status_out);
    return CXF_OK;
}
