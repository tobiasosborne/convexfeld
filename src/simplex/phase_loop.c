/**
 * @file phase_loop.c
 * @brief Phase I termination and Phase I→II transition.
 *
 * Contains cxf_check_phase_one_end which determines whether Phase I
 * has achieved feasibility and orchestrates the transition to Phase II.
 *
 * P1.1 (x5dj): Removed correct_basic_variables hack. Now uses the
 * Phase I surrogate objective directly, as the spec intends.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_compute_reduced_costs(SolverState *state);

/**
 * @brief Compute Phase I objective from scratch: sum of artificial values.
 *
 * Cross-checks state->obj_value (maintained incrementally) against a
 * fresh computation from work_x to detect numerical drift.
 */
static double compute_phase1_objective(SolverState *state) {
    int n = state->num_vars;
    int m = state->num_constrs;
    double obj = 0.0;
    for (int i = 0; i < m; i++) {
        int vi = n + i;
        if (state->work_obj[vi] > 0.0 && state->work_x[vi] > 0.0)
            obj += state->work_x[vi];
    }
    return obj;
}

/**
 * @brief Safety net: check if any nonbasic variable has improving reduced cost.
 *
 * This catches cases where the pricing subsystem (partial pricing) declared
 * ITERATE_OPTIMAL but a full scan finds candidates it missed. Keeps us from
 * declaring false INFEASIBLE due to pricing gaps.
 */
static int has_improving_direction(SolverState *state, CxfEnv *env) {
    int total = state->num_vars + state->num_constrs;
    for (int j = 0; j < total; j++) {
        if (state->basis->var_status[j] >= 0) continue;
        double lb = state->work_lb[j], ub = state->work_ub[j];
        if (ub <= lb + CXF_FEASIBILITY_TOL) continue;
        if (state->basis->var_status[j] == CXF_VAR_AT_LOWER &&
            state->work_dj[j] < -env->optimality_tol)
            return 1;
        if (state->basis->var_status[j] == CXF_VAR_AT_UPPER &&
            state->work_dj[j] > env->optimality_tol)
            return 1;
    }
    return 0;
}

/**
 * @brief Check Phase I termination and handle transition to Phase II.
 *
 * Called from the unified loop when cxf_simplex_step returns ITERATE_OPTIMAL
 * during Phase I. Uses the Phase I surrogate objective (sum of artificial
 * variable values) to determine feasibility.
 *
 * P1.1: No longer uses correct_basic_variables. The surrogate objective
 * is the correct signal — when pricing declares optimal and the objective
 * is zero, the basis IS feasible by construction.
 *
 * @return CXF_OK if transitioned to Phase II,
 *         CXF_INFEASIBLE if truly infeasible,
 *         1 if has improving direction (continue Phase I)
 */
int cxf_check_phase_one_end(SolverState *state, CxfModel *model, CxfEnv *env) {
    double tol = env->feasibility_tol;

    /* Compute Phase I objective fresh from work_x to guard against drift.
     * Use the larger of incremental and fresh as conservative check. */
    double fresh_obj = compute_phase1_objective(state);
    double phase1_obj = (fresh_obj > state->obj_value)
                        ? fresh_obj : state->obj_value;

    /* Reset incremental tracker to fresh value to prevent drift accumulation */
    state->obj_value = fresh_obj;

    if (phase1_obj <= tol) {
        /* Phase I objective is zero — feasible, transition to Phase II */
        extern int cxf_transition_to_phase_two(SolverState *, CxfModel *);
        int rc = cxf_transition_to_phase_two(state, model);
        if (rc != CXF_OK) return rc;
        cxf_compute_reduced_costs(state);
        return CXF_OK;
    }

    /* Phase I optimal but objective still positive.
     * Safety net: recompute reduced costs and do a full scan for improving
     * directions. Partial pricing may have missed candidates. */
    cxf_compute_reduced_costs(state);
    if (has_improving_direction(state, env)) return 1;

    /* No improving direction found by full scan either → truly infeasible */
    return CXF_INFEASIBLE;
}
