/**
 * @file simplex_cleanup.c
 * @brief Post-solve implied-bound tightening and resource deallocation.
 *
 * Implements cxf_simplex_cleanup per simplex_lifecycle.md:
 *   Phase 9: Constraint conversion (inequality -> equality for tight constraints)
 *   Phase 10: Memory cleanup (free temporary arrays)
 *
 * Phases 1-8 (basis index adjustment, variable classification, implied bound
 * computation, activity bounds, core propagation, variable fixing) are stubbed
 * and tracked under separate issues for incremental implementation.
 *
 * Spec: docs/specs-v2/specs/modules/simplex_lifecycle.md (cxf_simplex_cleanup)
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#include "simplex_internal.h"

/**
 * @brief Convert tight inequality constraints to equalities (Phase 9).
 *
 * Scans all active inequality constraints and checks whether both
 * activity bounds are tight (lower activity >= -feas_tol and upper
 * activity <= feas_tol). Constraints meeting this criterion are
 * converted to equalities and the pricing subsystem is notified.
 *
 * Per simplex_lifecycle.md Phase 9 and cleanup_utilities.md.
 */
static int convert_tight_constraints(SolverState *state, double feas_tol) {
    if (state->work_sense == NULL || state->min_activity == NULL ||
        state->max_activity == NULL)
        return 0;

    int converted = 0;
    int m = state->num_constrs;

    for (int i = 0; i < m; i++) {
        char sense = state->work_sense[i];

        /* Only convert inequalities, not existing equalities */
        if (sense == '=' || sense == 'E')
            continue;

        /* Check tightness: both activity bounds close to zero slack.
         * Per spec: lower activity >= -feas_tol AND upper activity
         * <= feas_tol indicates the constraint is effectively tight. */
        double min_act = state->min_activity[i];
        double max_act = state->max_activity[i];

        if (min_act >= -feas_tol && max_act <= feas_tol) {
            state->work_sense[i] = '=';
            converted++;
            state->ineq_to_eq_count++;

            /* Notify pricing subsystem of constraint conversion */
            if (state->pricing != NULL)
                cxf_pricing_mark_dirty(state->pricing, i);
        }
    }

    return converted;
}

/**
 * @brief Perform post-solve implied-bound tightening and cleanup.
 *
 * Per simplex_lifecycle.md, this is step 9 of the solve flow:
 *   7. cxf_simplex_refine  -- solution refinement
 *   8. cxf_simplex_final   -- dual-feasibility variable fixing
 *   9. cxf_simplex_cleanup -- implied-bound tightening + resource dealloc
 *
 * @param state Solver state (may be NULL for robustness)
 * @param env   Environment with tolerances (may be NULL)
 * @return 0 on success, error code on failure
 */
int cxf_simplex_cleanup(SolverState *state, CxfEnv *env) {
    if (state == NULL)
        return CXF_OK;  /* Nothing to clean up */

    double feas_tol = (env != NULL) ? env->feasibility_tol
                                    : CXF_FEASIBILITY_TOL;

    /* Phases 1-8: Basis index adjustment, variable classification,
     * implied bound computation, activity bounds, core propagation,
     * and variable fixing.
     * These are tracked under separate issues for incremental
     * implementation. In pure LP mode (var_flags all zero), Phase 1
     * (basis index adjustment) and Phase 6 (restoration) are no-ops. */

    /* T3.12: Recompute activity bounds AFTER simplex_final fixings.
     * Previous activity bounds are stale — simplex_final changed
     * variable values via pivot_bound. Fresh bounds detect constraints
     * that became tight from fixings. */
    cxf_compute_activity_bounds(state, 0, NULL);

    /* Phase 9: Constraint conversion */
    convert_tight_constraints(state, feas_tol);

    /* Phase 10: Free temporary working arrays.
     * The main resource deallocation is handled by cxf_simplex_final
     * (context.c), which frees all SolverState arrays. This phase
     * frees any cleanup-specific temporaries allocated during
     * Phases 1-8 (currently none, since those phases are stubbed). */

    return CXF_OK;
}
