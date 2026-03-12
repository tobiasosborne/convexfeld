/**
 * @file ratio_test.c
 * @brief Harris two-pass ratio test for simplex pivot selection.
 *
 * Implements cxf_ratio_test per harris_ratio_test.md (Maros 2003 Ch. 8).
 *
 * Pass 1: find theta_max = min { (slack + band) / |d_i| } over all rows.
 * Pass 2: among strict ratios <= theta_max, select largest pivot (or
 *          smallest index under Bland's rule).
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Compute Harris ratio for one basic variable against its bounds.
 *
 * Returns the minimum ratio to any active bound.  When band > 0 the
 * feasibility window is widened (Harris relaxation, Pass 1); band = 0
 * gives the strict ratio (Pass 2).
 *
 * @param x       Current value of basic variable.
 * @param lb      Lower bound.
 * @param ub      Upper bound.
 * @param sd      Signed pivot element (s * d_i), guaranteed nonzero.
 * @param band    Harris band width (harrisBand for Pass 1, 0 for Pass 2).
 * @param feasTol Primal feasibility tolerance.
 * @param inf     Infinity threshold.
 * @return Minimum valid ratio, or inf if no bound constrains movement.
 */
static double row_ratio(double x, double lb, double ub, double sd,
                        double band, double feasTol, double inf) {
    double r, ratio = inf;
    if (sd > 0) {
        /* Basic variable decreases: primary check is lower bound. */
        if (lb > -inf) {
            r = (x - lb + band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
        /* Phase I cross-bound: var already past upper bound. */
        if (ub < inf && x > ub + feasTol) {
            r = (x - ub + band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
    } else {
        /* Basic variable increases: primary check is upper bound. */
        if (ub < inf) {
            r = (x - ub - band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
        /* Phase I cross-bound: var already past lower bound. */
        if (lb > -inf && x < lb - feasTol) {
            r = (x - lb - band) / sd;
            if (r >= -feasTol && r < ratio) ratio = r;
        }
    }
    return ratio;
}

/**
 * @brief Perform Harris two-pass ratio test to select leaving variable.
 *
 * @param state          Solver context (basis, bounds, current solution).
 * @param env            Environment (tolerances).
 * @param enteringVar    Variable entering the basis.
 * @param pivotColumn    FTRAN result: B^{-1} a_entering (dense, length m).
 * @param columnNZ       Nonzero count (reserved for future sparse path).
 * @param leavingRow_out Output: row index of leaving variable.
 * @param pivotElement_out Output: pivot element value.
 * @return CXF_OK or CXF_UNBOUNDED.
 */
/**
 * @brief BFRT: find minimum-ratio non-flipped row (Stage 3 helper).
 *
 * Scans all non-flipped basic variables for the minimum non-negative
 * ratio.  Ties broken by largest |pivot element|.
 *
 * @return Row index of next blocker, or -1 if none.
 */
static int find_next_blocker(SolverState *state, const double *pivotCol,
                             const int *flipped, int num_flipped,
                             int entering_sign,
                             double *out_ratio, double *out_pivot) {
    int best_row = -1;
    double best_ratio = CXF_INFINITY;
    double best_pivot = 0.0;
    int m = state->num_constrs;
    BasisState *basis = state->basis;

    for (int i = 0; i < m; i++) {
        int skip = 0;
        for (int f = 0; f < num_flipped; f++)
            if (flipped[f] == i) { skip = 1; break; }
        if (skip) continue;

        double d_i = pivotCol[i];
        double sd_i = entering_sign * d_i;
        if (fabs(sd_i) < CXF_PIVOT_TOL) continue;

        int bv = basis->basic_vars[i];
        if (bv < 0) continue;

        double x = state->work_x[bv];
        double ratio;
        if (sd_i > 0)
            ratio = (x - state->work_lb[bv]) / sd_i;
        else
            ratio = (x - state->work_ub[bv]) / sd_i;

        if (ratio < -CXF_FEASIBILITY_TOL) continue;

        if (ratio < best_ratio - CXF_FEASIBILITY_TOL) {
            best_ratio = ratio; best_row = i; best_pivot = d_i;
        } else if (ratio <= best_ratio + CXF_FEASIBILITY_TOL) {
            if (fabs(d_i) > fabs(best_pivot)) {
                best_row = i; best_pivot = d_i;
            }
        }
    }

    if (out_ratio) *out_ratio = best_ratio;
    if (out_pivot) *out_pivot = best_pivot;
    return best_row;
}

#define MAX_BFRT_FLIPS 10

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
    const double band    = feasTol;           /* V2 spec harris_ratio_test.md line 188 */
    const int m     = state->num_constrs;
    const int total = state->num_vars + m;
    const int *bv   = state->basis->basic_vars;
    const double *wx  = state->work_x;
    const double *wlb = state->work_lb;
    const double *wub = state->work_ub;

    /* Entering direction: +1 from lower bound, -1 from upper bound.
     * Free/superbasic variables take direction from reduced cost sign. */
    int s = 1;
    if (state->basis && enteringVar >= 0) {
        int vs = state->basis->var_status[enteringVar];
        if (vs == CXF_VAR_AT_UPPER)
            s = -1;
        else if (vs == CXF_VAR_SUPERBASIC && state->work_dj &&
                 state->work_dj[enteringVar] > 0.0)
            s = -1;
    }

    /* V2 harris_ratio_test.md (Edge Cases: Infeasible Bounds Detected):
     * Pre-check each candidate's bound validity before ratio computation. */
    for (int i = 0; i < m; i++) {
        if (fabs(pivotColumn[i]) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;
        if (wlb[j] > wub[j] + feasTol)
            return CXF_INFEASIBLE;
    }

    /* --- Pass 1: theta_max = min relaxed ratio (harris_ratio_test.md) --- */
    double minRatio = inf;
    int minRow = -1;

    for (int i = 0; i < m; i++) {
        double d_i = pivotColumn[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
        int j = bv[i];
        if (j < 0 || j >= total) continue;

        double ratio = row_ratio(wx[j], wlb[j], wub[j],
                                 s * d_i, band, feasTol, inf);
        if (ratio < minRatio) {
            minRatio = ratio;
            minRow = i;
        }
    }

    if (minRow == -1) {
        if (status_out) *status_out = CXF_RT_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* --- Pass 2: best pivot among strict ratios <= theta_max --- */
    double maxPivot = fabs(pivotColumn[minRow]);
    int finalRow    = minRow;
    int blandBest   = bv[minRow];

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

    /* Compute initial step length theta for the Stage 2 blocker.
     * Check both bounds (Phase I: variable can be past opposite bound). */
    double theta;
    {
        int lv = bv[finalRow];
        double sd = s * pivotColumn[finalRow];
        theta = inf;
        if (lv >= 0 && lv < total) {
            if (sd > 0 && wlb[lv] > -inf) {
                double r = (wx[lv] - wlb[lv]) / sd;
                if (r >= 0 && r < theta) theta = r;
            }
            if (sd > 0 && wub[lv] < inf &&
                wx[lv] > wub[lv] + feasTol) {
                double r = (wx[lv] - wub[lv]) / sd;
                if (r >= 0 && r < theta) theta = r;
            }
            if (sd < 0 && wub[lv] < inf) {
                double r = (wx[lv] - wub[lv]) / sd;
                if (r >= 0 && r < theta) theta = r;
            }
            if (sd < 0 && wlb[lv] > -inf &&
                wx[lv] < wlb[lv] - feasTol) {
                double r = (wx[lv] - wlb[lv]) / sd;
                if (r >= 0 && r < theta) theta = r;
            }
        }
        if (theta >= inf) theta = 0;
        if (theta < 0) theta = 0;
    }

    /* --- Stage 3: BFRT / Long Step (harris_ratio_test.md) ---
     * Extend step via bound flips. Disabled under Bland's rule. */
    int local_flips[MAX_BFRT_FLIPS];
    int nflips = 0;
    int flip_cap = max_flips;
    if (flip_cap <= 0 || flip_cap > MAX_BFRT_FLIPS) flip_cap = MAX_BFRT_FLIPS;

    if (!state->use_bland && flip_rows_out) {
        int blocker_row = finalRow;
        while (nflips < flip_cap) {
            int bvar = bv[blocker_row];
            if (wlb[bvar] <= -inf || wub[bvar] >= inf ||
                (wub[bvar] - wlb[bvar]) < feasTol)
                break;
            local_flips[nflips] = blocker_row;
            if (flip_rows_out) flip_rows_out[nflips] = blocker_row;
            nflips++;
            double sd = fabs(s * pivotColumn[blocker_row]);
            if (sd < CXF_PIVOT_TOL) break;
            theta += (wub[bvar] - wlb[bvar]) / sd;
            double next_ratio, next_pivot;
            int next_row = find_next_blocker(state, pivotColumn, local_flips,
                                             nflips, s,
                                             &next_ratio, &next_pivot);
            if (next_row < 0) break;
            if (next_ratio < theta) theta = next_ratio;
            blocker_row = next_row;
            finalRow = next_row;
            *leavingRow_out = next_row;
            *pivotElement_out = next_pivot;
        }
    }
    if (num_flips_out) *num_flips_out = nflips;

    if (theta_out) *theta_out = theta;
    if (status_out) {
        if (nflips > 0 && *leavingRow_out == finalRow) {
            /* Check if final blocker was also flipped (all flipped) */
            int all_flipped = 0;
            for (int f = 0; f < nflips; f++)
                if (local_flips[f] == finalRow) { all_flipped = 1; break; }
            if (all_flipped)
                *status_out = CXF_RT_BOUND_FLIP_ONLY;
            else
                *status_out = (theta <= feasTol) ? CXF_RT_DEGENERATE_PIVOT
                                                 : CXF_RT_NORMAL_PIVOT;
        } else {
            *status_out = (theta <= feasTol) ? CXF_RT_DEGENERATE_PIVOT
                                             : CXF_RT_NORMAL_PIVOT;
        }
    }
    return CXF_OK;
}
