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
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"


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
    int total = n + m;

    /*--- Pass 1: Nonbasic variable cleanup ---
     * Only snap values to their CURRENT status bound (fix float drift).
     * Do NOT reassign status based on RC signs — that moves variables
     * to different bounds without pivoting, violating Ax=b.
     * The final accuracy pass (refactorize + recompute_xB) handles
     * constraint consistency after snapping. */
    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] >= 0) continue;
        if (basis->var_status[j] == CXF_VAR_FIXED) continue;

        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        /* Snap to current status bound (fix floating-point drift).
         * Create Variant 2 (variable-fixing) eta record per
         * simplex_phases.md for future warm-start/crossover recovery. */
        double snap_val = state->work_x[j];
        int snapped = 0;
        if (basis->var_status[j] == CXF_VAR_AT_LOWER && lb > -CXF_INFINITY) {
            snap_val = lb; snapped = 1;
        } else if (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                   ub < CXF_INFINITY) {
            snap_val = ub; snapped = 1;
        }
        if (snapped && snap_val != state->work_x[j]) {
            /* Create compact Variant 2 eta record */
            EtaVector *eta;
            if (basis->eta_pool != NULL)
                eta = (EtaVector *)cxf_eta_pool_alloc(basis->eta_pool,
                                                       sizeof(EtaVector));
            else
                eta = (EtaVector *)calloc(1, sizeof(EtaVector));
            if (eta != NULL) {
                eta->type = CXF_ETA_VARIABLE_FIX;
                eta->pivot_row = -1;
                eta->entering_var = j;
                eta->leaving_var = -1;
                eta->pivot_elem = snap_val;
                eta->reduced_cost = state->work_obj[j];
                eta->direction = 0;
                eta->status = basis->var_status[j];
                eta->nnz = 0;
                eta->indices = NULL;
                eta->values = NULL;
                eta->next = basis->eta_head;
                basis->eta_head = eta;
                basis->eta_count++;
            }
            state->work_x[j] = snap_val;
        }
    }

    /*--- Pass 2: Basic variable recovery ---
     * Disabled: recovery pivots change the basis and can move
     * the solution to a suboptimal vertex. The final accuracy
     * pass (recompute_xB) handles consistency without pivoting. */

    /*--- Pass 3: Objective recomputation ---
     * Skipped: the final accuracy pass (recompute_xB + recompute_objective)
     * does a full recomputation over ALL variables with consistent x_B. */

    if (state->work_counter)
        *state->work_counter += (double)(total + m);

    return CXF_OK;
}
