/**
 * @file bfrt.c
 * @brief BFRT (Bound-Flipping Ratio Test) Stage 3 / long step logic.
 *
 * Extracted from ratio_test.c.  Implements the flip loop and blocker
 * search per harris_ratio_test.md Section "Stage 3: Long Step".
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Find minimum-ratio non-flipped row (Stage 3 helper).
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

/**
 * @brief Extend step via BFRT bound flips (Stage 3).
 *
 * Starting from the Stage 2 blocker, iteratively flip bounded variables
 * at their opposite bound and search for the next blocker.  Disabled
 * under Bland's rule.
 *
 * @param state           Solver context.
 * @param pivotColumn     FTRAN result (dense, length m).
 * @param s               Entering direction (+1 or -1).
 * @param use_bland       If true, BFRT is disabled.
 * @param feasTol         Primal feasibility tolerance.
 * @param inf             Infinity threshold (env->infinity).
 * @param finalRow        Initial Stage 2 blocker row (updated on flips).
 * @param theta           Initial step length (updated on flips).
 * @param leavingRow_out  Updated leaving row after flips.
 * @param pivotElement_out Updated pivot element after flips.
 * @param flip_rows_out   Array to receive flipped row indices (may be NULL).
 * @param max_flips       Maximum flips allowed.
 * @param num_flips_out   Output: number of flips performed.
 */
void cxf_bfrt_extend_step(SolverState *state, const double *pivotColumn,
                           int s, int use_bland, double feasTol,
                           double inf,
                           int *finalRow, double *theta,
                           int *leavingRow_out, double *pivotElement_out,
                           int *flip_rows_out, int max_flips,
                           int *num_flips_out) {
    const int *bv = state->basis->basic_vars;
    const double *wlb = state->work_lb;
    const double *wub = state->work_ub;
    int local_flips[MAX_BFRT_FLIPS];
    int nflips = 0;
    int flip_cap = max_flips;

    if (flip_cap <= 0 || flip_cap > MAX_BFRT_FLIPS)
        flip_cap = MAX_BFRT_FLIPS;

    if (!use_bland && flip_rows_out) {
        int blocker_row = *finalRow;
        while (nflips < flip_cap) {
            int bvar = bv[blocker_row];
            if (wlb[bvar] <= -inf || wub[bvar] >= inf ||
                (wub[bvar] - wlb[bvar]) < feasTol)
                break;
            local_flips[nflips] = blocker_row;
            flip_rows_out[nflips] = blocker_row;
            nflips++;
            double sd = fabs(s * pivotColumn[blocker_row]);
            if (sd < CXF_PIVOT_TOL) break;
            *theta += (wub[bvar] - wlb[bvar]) / sd;
            double next_ratio, next_pivot;
            int next_row = find_next_blocker(state, pivotColumn, local_flips,
                                             nflips, s,
                                             &next_ratio, &next_pivot);
            if (next_row < 0) break;
            if (next_ratio < *theta) *theta = next_ratio;
            blocker_row = next_row;
            *finalRow = next_row;
            *leavingRow_out = next_row;
            *pivotElement_out = next_pivot;
        }
    }
    if (num_flips_out) *num_flips_out = nflips;
}

/**
 * @brief Determine ratio test status after BFRT.
 *
 * Sets the status output based on whether flips occurred and the
 * final theta value.
 *
 * @param nflips      Number of flips performed.
 * @param local_flips Array of flipped row indices.
 * @param finalRow    Final leaving row.
 * @param theta       Final step length.
 * @param feasTol     Primal feasibility tolerance.
 * @param status_out  Output: ratio test status enum.
 */
void cxf_bfrt_set_status(int nflips, const int *flip_rows,
                          int finalRow, double theta, double feasTol,
                          int *status_out) {
    if (!status_out) return;

    if (nflips > 0) {
        int all_flipped = 0;
        for (int f = 0; f < nflips; f++)
            if (flip_rows[f] == finalRow) { all_flipped = 1; break; }
        if (all_flipped) {
            *status_out = CXF_RT_BOUND_FLIP_ONLY;
            return;
        }
    }
    *status_out = (theta <= feasTol) ? CXF_RT_DEGENERATE_PIVOT
                                     : CXF_RT_NORMAL_PIVOT;
}
