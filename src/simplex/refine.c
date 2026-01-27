/**
 * @file refine.c
 * @brief Solution refinement for numerical cleanup (M7.1.12)
 *
 * Post-solve cleanup: snaps near-bound values, cleans zeros, recomputes objective.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#define NEAR_ZERO_TOL 1e-12

/**
 * @brief Refine solution for numerical stability
 *
 * Performs post-solve cleanup:
 * 1. Snaps primal values near bounds to exact bounds
 * 2. Cleans near-zero values in primal, dual, and reduced costs
 * 3. Recalculates objective value
 *
 * @param state Solver context with solution data
 * @param env Environment with tolerance settings
 * @return 0 if no adjustments made, 1 if adjustments made, negative on error
 */
int cxf_simplex_refine(SolverContext *state, CxfEnv *env) {
    /* Validate inputs */
    if (!state || !env) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Get tolerance from environment */
    double tol = env->feasibility_tol;
    int adjusted = 0;
    int n = state->num_vars;
    int m = state->num_constrs;

    /* Step 1: Snap primal values to bounds */
    for (int j = 0; j < n; j++) {
        if (fabs(state->work_x[j] - state->work_lb[j]) < tol) {
            state->work_x[j] = state->work_lb[j];
            adjusted++;
        } else if (fabs(state->work_x[j] - state->work_ub[j]) < tol) {
            state->work_x[j] = state->work_ub[j];
            adjusted++;
        }
    }

    /* Step 2: Clean near-zero values in primal variables */
    for (int j = 0; j < n; j++) {
        if (fabs(state->work_x[j]) < NEAR_ZERO_TOL) {
            state->work_x[j] = 0.0;
            adjusted++;
        }
    }

    /* Step 2: Clean near-zero values in dual variables (pi) */
    for (int i = 0; i < m; i++) {
        if (fabs(state->work_pi[i]) < NEAR_ZERO_TOL) {
            state->work_pi[i] = 0.0;
            adjusted++;
        }
    }

    /* Step 2: Clean near-zero values in reduced costs (dj) */
    for (int j = 0; j < n; j++) {
        if (fabs(state->work_dj[j]) < NEAR_ZERO_TOL) {
            state->work_dj[j] = 0.0;
            adjusted++;
        }
    }

    /* Step 3: Recalculate objective value */
    state->obj_value = 0.0;
    for (int j = 0; j < n; j++) {
        state->obj_value += state->work_obj[j] * state->work_x[j];
    }

    /* Return 1 if adjustments made, 0 otherwise */
    return (adjusted > 0) ? 1 : 0;
}
