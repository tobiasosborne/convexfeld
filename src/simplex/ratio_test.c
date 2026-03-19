/* ratio_test.c — Single-pass SE-weighted ratio test (T1.3) */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include "simplex_internal.h"
#include <math.h>

/* Strict theta: min non-negative ratio to any bound (both for Phase I) */
static double strict_theta(double x, double lb, double ub, double sd,
                           double feasTol, double inf) {
    double theta = inf;
    if (sd > 0) {
        if (lb > -inf) {
            double r = (x - lb) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
        if (ub < inf && x > ub + feasTol) {
            double r = (x - ub) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
    } else if (sd < 0) {
        if (ub < inf) {
            double r = (x - ub) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
        if (lb > -inf && x < lb - feasTol) {
            double r = (x - lb) / sd;
            if (r >= 0 && r < theta) theta = r;
        }
    }
    if (theta >= inf) theta = 0.0;
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
    const int m     = state->num_constrs;
    const int total = state->num_vars + m;
    const int *bv   = state->basis->basic_vars;
    const double *wx  = state->work_x;
    const double *wlb = state->work_lb;
    const double *wub = state->work_ub;

    const double *weights = (state->pricing && state->pricing->weights)
                            ? state->pricing->weights : NULL;
    int num_wt = weights ? state->pricing->num_vars : 0;

    int s = 1;
    if (state->basis && enteringVar >= 0) {
        int vs = state->basis->var_status[enteringVar];
        if (vs == CXF_VAR_AT_UPPER) s = -1;
        else if (vs == CXF_VAR_SUPERBASIC && state->work_dj &&
                 state->work_dj[enteringVar] > 0.0) s = -1;
    }
    double rc_enter = (state->work_dj && enteringVar >= 0 &&
                       enteringVar < total)
                      ? state->work_dj[enteringVar] : 0.0;

    int bestRow = -1;
    double bestScore = inf, bestPivot = 0.0;
    int blandBest = total + 1;

    for (int i = 0; i < m; i++) {
        double d_i = pivotColumn[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) continue;

        int j = bv[i];
        if (j < 0 || j >= total) continue;

        if (wlb[j] > wub[j] + feasTol) return CXF_INFEASIBLE;
        double sd = s * d_i, ratio = inf;
        int is_eq = (wlb[j] > -inf && wub[j] < inf &&
                     wub[j] - wlb[j] <= CXF_BOUND_EQUALITY_TOL);
        if (is_eq) {
            double r = fabs(wx[j] - wlb[j]) / fabs(sd);
            if (r < ratio) ratio = r;
        } else if (sd > 0) {
            if (wlb[j] > -inf) {
                double r = (wx[j] - wlb[j] + feasTol) / sd;
                if (r >= 0 && r < ratio) ratio = r;
            }
            if (wub[j] < inf && wx[j] > wub[j] + feasTol) {
                double r = (wx[j] - wub[j] + feasTol) / sd;
                if (r >= 0 && r < ratio) ratio = r;
            }
        } else {
            if (wub[j] < inf) {
                double r = (wub[j] - wx[j] + feasTol) / (-sd);
                if (r >= 0 && r < ratio) ratio = r;
            }
            if (wlb[j] > -inf && wx[j] < wlb[j] - feasTol) {
                double r = (wlb[j] - wx[j] + feasTol) / (-sd);
                if (r >= 0 && r < ratio) ratio = r;
            }
        }
        if (ratio >= inf) continue;

        double w = (weights && j < num_wt && weights[j] > 1e-10)
                   ? sqrt(weights[j]) : 1.0;
        double dir = (fabs(rc_enter) > feasTol && rc_enter * d_i > 0)
                     ? 2.0 : 1.0;
        double score = ratio * dir / w;
        int better = 0;
        if (state->use_bland) {
            if (score < bestScore - feasTol)
                better = 1;
            else if (score <= bestScore + feasTol && j < blandBest)
                better = 1;
        } else {
            if (score < bestScore - feasTol)
                better = 1;
            else if (score <= bestScore + feasTol &&
                     fabs(d_i) > bestPivot)
                better = 1;
        }

        if (better) {
            bestRow = i;
            bestScore = score;
            bestPivot = fabs(d_i);
            blandBest = j;
        }
    }

    if (bestRow == -1) {
        if (status_out) *status_out = CXF_RT_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* Strict (unrelaxed) theta for the leaving row */
    int lv = bv[bestRow];
    double sd_best = s * pivotColumn[bestRow];
    double theta = strict_theta(wx[lv], wlb[lv], wub[lv], sd_best, feasTol, inf);

    /* === Single-flip BFRT === */
    int nflips = 0;
    if (!state->use_bland && flip_rows_out && max_flips > 0 &&
        lv >= 0 && lv < total &&
        wlb[lv] > -inf && wub[lv] < inf &&
        (wub[lv] - wlb[lv]) > feasTol) {
        /* Blocking variable can flip — record and extend theta */
        flip_rows_out[0] = bestRow;
        nflips = 1;
        double flip_ext = (wub[lv] - wlb[lv]) / fabs(sd_best);
        theta += flip_ext;

        /* Find next blocker after the flip (in bfrt.c) */
        int nextRow = -1;
        double nextRatio = inf;
        nextRow = cxf_bfrt_next_blocker(state, pivotColumn, s,
                                         bestRow, feasTol, &nextRatio);
        if (nextRow >= 0 && nextRatio <= theta) {
            theta = nextRatio;
            bestRow = nextRow;
        }
    }

    *leavingRow_out  = bestRow;
    *pivotElement_out = pivotColumn[bestRow];
    if (theta_out) *theta_out = theta;
    if (num_flips_out) *num_flips_out = nflips;

    /* Status determination */
    if (status_out) {
        if (nflips > 0 && bestRow == flip_rows_out[0])
            *status_out = CXF_RT_BOUND_FLIP_ONLY;
        else
            *status_out = (theta <= feasTol) ? CXF_RT_DEGENERATE_PIVOT
                                              : CXF_RT_NORMAL_PIVOT;
    }
    return CXF_OK;
}
