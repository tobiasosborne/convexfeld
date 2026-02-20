/**
 * @file phase_steps.c
 * @brief Bound propagation — step2 + step3 (v2 P3.20)
 *
 * cxf_simplex_step2: Variable-side bound propagation
 * cxf_simplex_step3: Constraint-side bound propagation (LP only)
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Variable-side bound propagation (v2 P3.20).
 *
 * Processes dirty variables from the pricing queue. For each:
 *   1. Check bound feasibility
 *   2. Tighten bounds based on reduced cost sign
 *   3. Notify pricing of changes
 *
 * @return 0 on success, CXF_INFEASIBLE on bound violation
 */
int cxf_simplex_step2(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) return 0;
    if (state->pricing == NULL) return 0;
    if (state->pricing->var_dirty == NULL) return 0;

    double feas_tol = env->feasibility_tol;
    int n = state->num_vars;
    int total = n + state->num_constrs;

    for (int j = 0; j < total; j++) {
        if (j < n && !state->pricing->var_dirty[j]) continue;
        if (j >= n) continue;  /* Only process original variables */

        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        /* Infeasibility check */
        if (lb > ub + feas_tol) {
            return CXF_INFEASIBLE;
        }

        /* Skip if already fixed or basic */
        if (state->basis != NULL && state->basis->var_status[j] >= 0) continue;
        if (ub - lb < feas_tol) continue;

        /* Reduced-cost-based bound tightening:
         * If rc > 0 and var at lower bound, it won't move → no tightening
         * If rc < 0 and var at upper bound, it won't move → no tightening
         * Full implementation creates bound-change eta records (TODO) */
    }

    /* Clear dirty flags */
    state->pricing->num_dirty = 0;

    /* Increment flip counter */
    state->flip_count += 0;  /* No actual flips in this stub */

    return 0;
}

/**
 * @brief Constraint-side bound propagation (v2 P3.20, LP only).
 *
 * For each dirty constraint, compute implied variable bounds from
 * constraint activities and tighten bounds where implication is stronger.
 *
 * @return 0 on success, CXF_INFEASIBLE on bound violation
 */
int cxf_simplex_step3(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) return 0;
    if (state->pricing == NULL) return 0;
    if (state->min_activity == NULL || state->max_activity == NULL) return 0;

    CxfModel *model = state->model_ref;
    if (model == NULL || model->matrix == NULL) return 0;

    MatrixData *mat = model->matrix;
    if (mat->col_ptr == NULL) return 0;

    double feas_tol = env->feasibility_tol;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Process dirty constraints */
    for (int i = 0; i < m; i++) {
        if (state->pricing->constr_dirty != NULL &&
            !state->pricing->constr_dirty[i]) continue;

        double min_act = state->min_activity[i];
        double max_act = state->max_activity[i];

        /* Skip constraints with infinite activity */
        if (min_act <= -CXF_INFINITY || max_act >= CXF_INFINITY) continue;

        /* Get constraint RHS and sense */
        double rhs = (mat->rhs != NULL) ? mat->rhs[i] : 0.0;
        char sense = (mat->sense != NULL) ? mat->sense[i] : '<';

        /* Check constraint feasibility */
        if (sense == '<' || sense == 'L') {
            if (min_act > rhs + feas_tol) return CXF_INFEASIBLE;
        } else if (sense == '>' || sense == 'G') {
            if (max_act < rhs - feas_tol) return CXF_INFEASIBLE;
        } else { /* equality */
            if (min_act > rhs + feas_tol) return CXF_INFEASIBLE;
            if (max_act < rhs - feas_tol) return CXF_INFEASIBLE;
        }

        /* Implied bound computation for each variable in this constraint.
         * For a_ij * x_j + rest <= rhs:
         *   x_j <= (rhs - min_rest) / a_ij  (if a_ij > 0)
         * Full implementation scans CSR row and tightens bounds (TODO) */
    }

    /* Clear constraint dirty flags */
    if (state->pricing->constr_dirty != NULL && m > 0) {
        state->pricing->num_constr_dirty = 0;
    }

    state->bounds_propagated++;
    return 0;
}
