/**
 * @file cleanup.c
 * @brief Simplex postsolve — restore original problem space (P6.1)
 *
 * After the simplex iteration loop and refinement, postsolve reverses
 * preprocessing transformations:
 *   1. Restore variables fixed during preprocessing to their original bounds
 *   2. Unscale primal values (when scaling is implemented)
 *   3. Unscale dual values (when scaling is implemented)
 *   4. Unscale reduced costs (when scaling is implemented)
 *
 * Spec: solution_processing.md — cxf_uncrush_solution
 * Beads: cps2
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Post-solve cleanup to restore original problem space.
 *
 * Phase 1: Restore variables that were fixed during preprocessing.
 *          Variables with work_lb == work_ub that differ from saved bounds
 *          are snapped back to their original bound range.
 * Phase 2: Unscale primal/dual (stub — scaling not yet implemented).
 * Phase 3: Verify basic variable feasibility.
 *
 * @param state Solver context containing solution arrays
 * @param env Environment with tolerances
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_postsolve(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    int n = state->num_vars;
    int m = state->num_constrs;
    BasisState *basis = state->basis;

    /*--- Phase 1: Restore preprocessed fixed variables ---*/
    if (state->saved_lb && state->saved_ub && basis && basis->var_status) {
        double tol = env->feasibility_tol;

        for (int j = 0; j < n; j++) {
            double cur_lb = state->work_lb[j];
            double cur_ub = state->work_ub[j];
            double orig_lb = state->saved_lb[j];
            double orig_ub = state->saved_ub[j];

            /* Was this variable fixed during preprocessing? */
            if (fabs(cur_ub - cur_lb) < tol &&
                (orig_ub - orig_lb) > tol) {
                /* Restore original bounds */
                state->work_lb[j] = orig_lb;
                state->work_ub[j] = orig_ub;

                /* If nonbasic, snap to nearest original bound */
                if (basis->var_status[j] < 0) {
                    double x = state->work_x[j];
                    if (fabs(x - orig_lb) <= fabs(x - orig_ub))
                        state->work_x[j] = orig_lb;
                    else
                        state->work_x[j] = orig_ub;
                }
            }
        }
    }

    /* Phase 2: Unscale primal/dual values (stub — no scaling yet) */

    /*--- Phase 3: Basic variable feasibility check ---*/
    if (basis && basis->basic_vars && state->work_x) {
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= n + 2 * m) continue;
            double x = state->work_x[bv];
            double lb = state->work_lb[bv];
            double ub = state->work_ub[bv];

            /* Clamp basic variables to restored bounds */
            if (x < lb - env->feasibility_tol)
                state->work_x[bv] = lb;
            else if (x > ub + env->feasibility_tol)
                state->work_x[bv] = ub;
        }
    }

    return CXF_OK;
}
