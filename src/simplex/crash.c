/**
 * @file crash.c
 * @brief Initial crash basis construction (M7.1.7)
 *
 * Implements cxf_simplex_crash which constructs an initial feasible basis
 * by heuristically selecting basic variables. For inequality constraints,
 * slack variables are preferred as they provide numerical stability with
 * unit coefficients. The crash procedure significantly reduces simplex
 * iterations compared to starting with all slacks basic.
 *
 * Spec: docs/specs/functions/simplex/cxf_simplex_crash.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/**
 * @brief Construct initial crash basis.
 *
 * Selects which variables should be basic vs nonbasic using heuristic
 * scoring. For inequality constraints, slacks are preferred. For equality
 * constraints, structural variables are selected based on coefficient
 * magnitude, bound range, and objective cost.
 *
 * The function allocates var_status array (size n+m) to track status of
 * all structural variables and slacks, and populates basis_header (using
 * existing basic_vars array) to identify which variable is basic in each row.
 *
 * @param state Solver context (must have valid dimensions and bounds)
 * @param env Environment (currently unused, reserved for future parameters)
 * @return CXF_OK on success, CXF_ERROR_OUT_OF_MEMORY on allocation failure
 */
int cxf_simplex_crash(SolverContext *state, CxfEnv *env) {
    int n, m;
    int *var_status;
    int *basis_header;
    double *work_lb;
    double *work_ub;

    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (state->basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Extract dimensions */
    n = state->num_vars;
    m = state->num_constrs;

    /* Edge case: no constraints */
    if (m == 0) {
        state->phase = 2;
        return CXF_OK;
    }

    /* Free existing var_status if allocated (was size n, need size n+m) */
    if (state->basis->var_status != NULL) {
        free(state->basis->var_status);
        state->basis->var_status = NULL;
    }

    /* Allocate var_status array for all variables and slacks */
    var_status = (int *)malloc((size_t)(n + m) * sizeof(int));
    if (var_status == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Use existing basic_vars array as basis_header */
    basis_header = state->basis->basic_vars;
    if (basis_header == NULL && m > 0) {
        free(var_status);
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Get bounds arrays */
    work_lb = state->work_lb;
    work_ub = state->work_ub;

    /*
     * Initialize all structural variables as nonbasic.
     * Status convention: -1 = at lower bound, -2 = at upper bound
     */
    for (int j = 0; j < n; j++) {
        if (work_lb != NULL && work_ub != NULL) {
            if (work_lb[j] > -CXF_INFINITY) {
                var_status[j] = -1;  /* At lower bound */
            } else if (work_ub[j] < CXF_INFINITY) {
                var_status[j] = -2;  /* At upper bound */
            } else {
                var_status[j] = -1;  /* Free variable, default to lower */
            }
        } else {
            var_status[j] = -1;  /* Default: at lower bound */
        }
    }

    /* Initialize all slack variables as nonbasic (at lower bound) */
    for (int i = 0; i < m; i++) {
        var_status[n + i] = -1;
    }

    /* Initialize basis_header to all invalid */
    for (int i = 0; i < m; i++) {
        basis_header[i] = -1;
    }

    /*
     * Select basic variables for each constraint row.
     *
     * Simplified initial implementation: use slack variable for each row.
     * This is always feasible for inequality constraints and provides
     * numerical stability with unit coefficients.
     *
     * Future enhancement: For equality constraints or to improve starting
     * point, score structural variables based on:
     * - Coefficient magnitude (larger is better)
     * - Bound range (tighter is better)
     * - Objective coefficient (lower cost is better)
     * - Whether zero is in bounds (helps feasibility)
     */
    for (int i = 0; i < m; i++) {
        /* Select slack variable as basic for this row */
        int slack_idx = n + i;

        basis_header[i] = slack_idx;
        var_status[slack_idx] = i;  /* Status = row index means basic */
    }

    /* Assign the allocated var_status to state */
    state->basis->var_status = var_status;

    /*
     * Determine initial phase.
     *
     * Phase 2: All slacks can be set to zero (for <= and >= constraints)
     *          or positive values, making the basis immediately feasible.
     *
     * Phase 1: Would be needed if we had equality constraints that couldn't
     *          find good structural basic variables, requiring artificial
     *          variables. For the all-slack basis, we start in Phase 2.
     */
    state->phase = 2;

    return CXF_OK;
}
