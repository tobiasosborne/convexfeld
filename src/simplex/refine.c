/**
 * @file refine.c
 * @brief Post-solve solution refinement (v2 P3.21)
 *
 * After the main iteration loop terminates, refine cleans up the
 * solution: fixes nonbasic variables at appropriate bounds based on
 * reduced cost sign, recovers basic variables near upper bounds.
 *
 * Spec: docs/specs-v2/specs/modules/simplex_phases.md (cxf_simplex_refine)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

extern int cxf_pivot_primal(void *env, void *state, int var, double tol);

/**
 * @brief Refine solution after simplex termination (v2 P3.21).
 *
 * Pass 1: Nonbasic variable cleanup — fix at appropriate bounds
 *         based on reduced cost sign.
 * Pass 2: Basic variable recovery — variables near upper bounds
 *         are processed via cxf_pivot_primal.
 * Pass 3: Recompute objective value.
 *
 * @return 0 on success, CXF_INFEASIBLE/CXF_UNBOUNDED on error
 */
int cxf_simplex_refine(SolverState *state, CxfEnv *env) {
    if (!state || !env) return CXF_ERROR_NULL_ARGUMENT;

    BasisState *basis = state->basis;
    if (!basis || !basis->var_status) return CXF_OK;

    double tol = env->feasibility_tol;
    double dual_tol = env->optimality_tol;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + 2 * m;

    /*--- Pass 1: Nonbasic variable cleanup ---*/
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] >= 0) continue;  /* Skip basic */
        if (basis->var_status[j] == CXF_VAR_FIXED) continue;

        double rc = (state->work_dj && j < total) ? state->work_dj[j] : 0.0;
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        /* Skip if already at correct bound */
        if (fabs(rc) <= dual_tol) {
            /* Near-zero RC: snap to nearest bound */
            double x = state->work_x[j];
            if (fabs(x - lb) < fabs(x - ub)) {
                state->work_x[j] = lb;
                basis->var_status[j] = CXF_VAR_AT_LOWER;
            } else if (ub < CXF_INFINITY) {
                state->work_x[j] = ub;
                basis->var_status[j] = CXF_VAR_AT_UPPER;
            }
            continue;
        }

        if (rc > dual_tol) {
            /* Positive RC (minimization): fix at lower bound */
            if (lb > -CXF_INFINITY) {
                state->work_x[j] = lb;
                basis->var_status[j] = CXF_VAR_AT_LOWER;
            } else {
                return CXF_UNBOUNDED;
            }
        } else {
            /* Negative RC: fix at upper bound */
            if (ub < CXF_INFINITY) {
                state->work_x[j] = ub;
                basis->var_status[j] = CXF_VAR_AT_UPPER;
            } else {
                return CXF_UNBOUNDED;
            }
        }
    }

    /*--- Pass 2: Basic variable recovery ---*/
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv < 0 || bv >= total) continue;

        double x = state->work_x[bv];
        double ub = state->work_ub[bv];

        /* If basic variable is near its upper bound, recover via pivot */
        if (ub < CXF_INFINITY && fabs(x - ub) < tol && bv < n) {
            int rc = cxf_pivot_primal(env, state, bv, tol);
            if (rc != 0 && rc != 3) return rc;
            /* rc == 3 (infeasible for this var) is ok — skip it */
        }
    }

    /*--- Pass 3: Recompute objective ---*/
    if (state->work_obj && state->work_x) {
        double obj = 0.0;
        for (int j = 0; j < n; j++)
            obj += state->work_obj[j] * state->work_x[j];
        state->obj_value = obj;
    }

    if (state->work_counter)
        *state->work_counter += (double)(total + m);

    return CXF_OK;
}
