/**
 * @file ratio_test.c
 * @brief Harris two-pass ratio test for simplex pivot selection.
 *
 * Implements cxf_ratio_test as specified in:
 * docs/specs/functions/ratio_test/cxf_ratio_test.md
 *
 * The ratio test determines which basic variable should leave the basis
 * during a simplex pivot. Uses Harris two-pass approach for numerical
 * stability: first pass finds minimum ratio with relaxed tolerance,
 * second pass selects largest pivot magnitude among near-minimum ratios.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Perform Harris two-pass ratio test to select leaving variable.
 *
 * Determines which basic variable reaches its bound first as the entering
 * variable increases. Implements numerical stability via two-pass approach:
 * 1. Find minimum ratio with relaxed tolerance (10x feasibility tolerance)
 * 2. Select largest pivot magnitude among ratios within threshold of minimum
 *
 * @param state Solver context containing basis, bounds, and current solution
 * @param env Environment containing tolerance parameters
 * @param enteringVar Index of variable entering the basis
 * @param pivotColumn BTRAN result: B^-1 A_entering in dense format
 * @param columnNZ Number of nonzeros in pivot column (unused in dense impl)
 * @param leavingRow_out Output: row index of leaving variable
 * @param pivotElement_out Output: pivot element value
 * @return CXF_OK on success, CXF_UNBOUNDED if no variable reaches bound
 */
int cxf_ratio_test(SolverContext *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out) {
    double feasTol, infinity, relaxedTol;
    double minRatio, threshold, maxPivot;
    int minRow, finalRow;
    int i, basicVar;
    double d_i, x_i, lb, ub, ratio;

    /* Suppress unused parameter warnings (for future sparse impl) */
    (void)enteringVar;
    (void)columnNZ;

    /* Validate inputs */
    if (state == NULL || env == NULL || pivotColumn == NULL ||
        leavingRow_out == NULL || pivotElement_out == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Extract tolerances from environment */
    feasTol = env->feasibility_tol;
    infinity = env->infinity;
    relaxedTol = 10.0 * feasTol;

    /* Initialize for first pass */
    minRatio = infinity;
    minRow = -1;

    /*
     * First pass: Find minimum ratio with relaxed tolerance.
     * Skip near-zero pivot elements to avoid numerical instability.
     */
    for (i = 0; i < state->num_constrs; i++) {
        d_i = pivotColumn[i];

        /* Skip near-zero pivot elements */
        if (fabs(d_i) <= relaxedTol) {
            continue;
        }

        /* Get basic variable at this row */
        basicVar = state->basis->basic_vars[i];

        /* Skip invalid variable indices
         * Valid range: [0, num_vars + num_constrs) to include artificials */
        int total_vars = state->num_vars + state->num_constrs;
        if (basicVar < 0 || basicVar >= total_vars) {
            continue;
        }

        /* Get current value and bounds */
        x_i = state->work_x[basicVar];
        lb = state->work_lb[basicVar];
        ub = state->work_ub[basicVar];

        /* Compute ratio based on sign of pivot coefficient.
         * When entering var increases by theta, basic var changes by -theta * d_i.
         * - If d_i > 0: basic var decreases, hits lower bound
         * - If d_i < 0: basic var increases, hits upper bound
         */
        if (d_i > relaxedTol) {
            /* Positive coefficient: basic var decreases toward lower bound */
            if (lb <= -infinity) {
                continue;  /* Lower bound is infinite */
            }
            ratio = (x_i - lb) / d_i;
        } else if (d_i < -relaxedTol) {
            /* Negative coefficient: basic var increases toward upper bound */
            if (ub >= infinity) {
                continue;  /* Upper bound is infinite */
            }
            ratio = (x_i - ub) / d_i;  /* d_i < 0 makes this positive */
        } else {
            continue;  /* Coefficient too small */
        }

        /* Update minimum if this ratio is smaller */
        if (ratio >= -feasTol && ratio < minRatio) {
            minRatio = ratio;
            minRow = i;
        }
    }

    /* Check for unboundedness */
    if (minRow == -1) {
        return CXF_UNBOUNDED;
    }

    /*
     * Second pass: Select largest pivot magnitude among near-minimum ratios.
     * This improves numerical stability by avoiding tiny pivot elements.
     */
    threshold = minRatio + feasTol;
    maxPivot = fabs(pivotColumn[minRow]);
    finalRow = minRow;

    for (i = 0; i < state->num_constrs; i++) {
        d_i = pivotColumn[i];

        /* Skip near-zero pivot elements */
        if (fabs(d_i) <= relaxedTol) {
            continue;
        }

        /* Get basic variable at this row */
        basicVar = state->basis->basic_vars[i];

        /* Skip invalid variable indices (include artificials) */
        int total_vars2 = state->num_vars + state->num_constrs;
        if (basicVar < 0 || basicVar >= total_vars2) {
            continue;
        }

        /* Get current value and bounds */
        x_i = state->work_x[basicVar];
        lb = state->work_lb[basicVar];
        ub = state->work_ub[basicVar];

        /* Compute ratio (same logic as first pass) */
        if (d_i > relaxedTol) {
            /* Positive coefficient: basic var decreases toward lower bound */
            if (lb <= -infinity) {
                continue;
            }
            ratio = (x_i - lb) / d_i;
        } else if (d_i < -relaxedTol) {
            /* Negative coefficient: basic var increases toward upper bound */
            if (ub >= infinity) {
                continue;
            }
            ratio = (x_i - ub) / d_i;
        } else {
            continue;
        }

        /* If ratio is within threshold, consider this pivot */
        if (ratio <= threshold) {
            if (fabs(d_i) > maxPivot) {
                maxPivot = fabs(d_i);
                finalRow = i;
            }
        }
    }

    /* Set output parameters */
    *leavingRow_out = finalRow;
    *pivotElement_out = pivotColumn[finalRow];

    return CXF_OK;
}
