/**
 * @file cleanup_kahan.c
 * @brief Kahan (compensated) summation for activity bound computation.
 *
 * Provides numerically stable activity computation for cleanup bound
 * propagation (T2.8, bound_propagation.md Section 3/7).
 *
 * Conservative rounding: max_activity nudges away from zero (never
 * underestimate), min_activity nudges toward zero (never overestimate).
 */

#include "convexfeld/cxf_types.h"
#include <math.h>

#define KAHAN_RHO_UP   (1.0 + 1e-12)
#define KAHAN_RHO_DOWN (1.0 - 1e-12)

/**
 * @brief Kahan compensated addition with conservative rounding.
 *
 * Detects cancellation and applies directional correction.
 * @param S      Current sum
 * @param delta  Increment
 * @param is_max Nonzero for max_activity (round up), 0 for min
 * @return       Corrected S + delta
 */
double cxf_kahan_activity_sum(double S, double delta, int is_max) {
    double S_new = S + delta;

    int cancel = 0;
    if (fabs(S) < fabs(delta))
        cancel = ((S_new - delta) != S);
    else
        cancel = ((S_new - S) != delta);

    if (cancel) {
        if (is_max) {
            S_new = (S_new >= 0.0) ? S_new * KAHAN_RHO_UP
                                   : S_new * KAHAN_RHO_DOWN;
        } else {
            S_new = (S_new >= 0.0) ? S_new * KAHAN_RHO_DOWN
                                   : S_new * KAHAN_RHO_UP;
        }
    }

    return S_new;
}

/**
 * @brief Compute activity bounds for one CSR row using Kahan summation.
 *
 * @param row_start  CSR row_ptr[i]
 * @param row_end    CSR row_ptr[i+1]
 * @param col_idx    CSR column indices
 * @param values     CSR coefficient values
 * @param work_lb    Variable lower bounds
 * @param work_ub    Variable upper bounds
 * @param out_min    Output: min activity (finite part)
 * @param out_max    Output: max activity (finite part)
 * @param out_neg_c  Output: negUnbdCount (NULL-safe)
 * @param out_pos_c  Output: posUnbdCount (NULL-safe)
 * @param total      Total number of variables (bounds checking)
 */
void cxf_kahan_row_activity(int64_t row_start, int64_t row_end,
                            const int *col_idx, const double *values,
                            const double *work_lb, const double *work_ub,
                            double *out_min, double *out_max,
                            int *out_neg_c, int *out_pos_c,
                            int total) {
    double min_a = 0.0, max_a = 0.0;
    int neg_c = 0, pos_c = 0;

    for (int64_t k = row_start; k < row_end; k++) {
        int j = col_idx[k];
        if (j < 0 || j >= total) continue;

        double a = values[k];
        double lb = work_lb[j];
        double ub = work_ub[j];

        /* Min activity contribution */
        if (a > 0.0) {
            if (lb <= -CXF_INFINITY) neg_c++;
            else min_a = cxf_kahan_activity_sum(min_a, a * lb, 0);
        } else if (a < 0.0) {
            if (ub >= CXF_INFINITY) neg_c++;
            else min_a = cxf_kahan_activity_sum(min_a, a * ub, 0);
        }

        /* Max activity contribution */
        if (a > 0.0) {
            if (ub >= CXF_INFINITY) pos_c++;
            else max_a = cxf_kahan_activity_sum(max_a, a * ub, 1);
        } else if (a < 0.0) {
            if (lb <= -CXF_INFINITY) pos_c++;
            else max_a = cxf_kahan_activity_sum(max_a, a * lb, 1);
        }
    }

    *out_min = min_a;
    *out_max = max_a;
    if (out_neg_c) *out_neg_c = neg_c;
    if (out_pos_c) *out_pos_c = pos_c;
}
