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
int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out) {
    double feasTol, infinity, relaxedTol;
    double minRatio, threshold, maxPivot;
    int minRow, finalRow;
    int i, basicVar;
    double d_i, x_i, lb, ub, ratio;

    /* Suppress unused parameter warning (for future sparse impl) */
    (void)columnNZ;

    /* Validate inputs */
    if (state == NULL || env == NULL || pivotColumn == NULL ||
        leavingRow_out == NULL || pivotElement_out == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Extract tolerances from environment */
    feasTol = env->feasibility_tol;
    infinity = env->infinity;
    /* P0.6: Use CXF_PIVOT_TOL (1e-9) for element skip, not 10*feasTol (1e-5).
     * Spec: tolerances_constants.md §3 — Harris pivot threshold ~1e-9 */
    relaxedTol = CXF_PIVOT_TOL;

    /* Entering direction: +1 if entering from lower bound (increases),
     * -1 if entering from upper bound (decreases).
     * Spec: harris_ratio_test.md Stage 1 "Entering direction" */
    int s = 1;
    if (state->basis != NULL && enteringVar >= 0 &&
        state->basis->var_status[enteringVar] == CXF_VAR_AT_UPPER)
        s = -1;

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

        /* Compute ratio using effective pivot direction s * d_i.
         * When entering var moves by theta, basic var changes by -s*theta*d_i.
         * Using sd_i = s * d_i:
         * - If sd_i > 0: basic var decreases, hits lower bound
         * - If sd_i < 0: basic var increases, hits upper bound
         */
        double sd_i = s * d_i;
        if (sd_i > relaxedTol) {
            if (lb <= -infinity) {
                continue;  /* Lower bound is infinite */
            }
            ratio = (x_i - lb) / sd_i;
        } else if (sd_i < -relaxedTol) {
            if (ub >= infinity) {
                continue;  /* Upper bound is infinite */
            }
            ratio = (x_i - ub) / sd_i;  /* sd_i < 0 makes this positive */
        } else {
            continue;  /* Effective coefficient too small */
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
     * Second pass: Among near-minimum ratios, select by:
     * - Bland's rule: smallest variable index (anti-cycling guarantee)
     * - Normal: largest pivot magnitude (numerical stability)
     */
    threshold = minRatio + feasTol;
    maxPivot = fabs(pivotColumn[minRow]);
    finalRow = minRow;
    int bland_best_var = state->basis->basic_vars[minRow];

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

        /* Compute ratio (same s * d_i logic as first pass) */
        double sd_i2 = s * d_i;
        if (sd_i2 > relaxedTol) {
            if (lb <= -infinity) {
                continue;
            }
            ratio = (x_i - lb) / sd_i2;
        } else if (sd_i2 < -relaxedTol) {
            if (ub >= infinity) {
                continue;
            }
            ratio = (x_i - ub) / sd_i2;
        } else {
            continue;
        }

        /* If ratio is within threshold, consider this pivot */
        if (ratio <= threshold) {
            if (state->use_bland) {
                /* Bland's leaving rule: smallest variable index breaks ties */
                if (basicVar < bland_best_var) {
                    bland_best_var = basicVar;
                    finalRow = i;
                }
            } else {
                if (fabs(d_i) > maxPivot) {
                    maxPivot = fabs(d_i);
                    finalRow = i;
                }
            }
        }
    }

    /* Set output parameters */
    *leavingRow_out = finalRow;
    *pivotElement_out = pivotColumn[finalRow];

    return CXF_OK;
}
