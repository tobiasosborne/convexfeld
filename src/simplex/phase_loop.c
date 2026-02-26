/**
 * @file phase_loop.c
 * @brief Phase I termination and Phase I→II transition.
 *
 * Contains cxf_check_phase_one_end which determines whether Phase I
 * has achieved feasibility and orchestrates the transition to Phase II.
 *
 * Phase I uses implicit bound-violation approach per two_phase_method.md.
 * Phase I objective = sum of basic variable bound violations.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"


/**
 * @brief Compute Phase I objective: sum of basic variable bound violations.
 */
static double compute_phase1_objective(SolverState *state) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    double obj = 0.0;
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        double x = state->work_x[bv];
        double lb = state->work_lb[bv];
        double ub = state->work_ub[bv];
        if (x < lb) obj += (lb - x);
        else if (x > ub) obj += (x - ub);
    }
    return obj;
}

/**
 * @brief Safety net: check if any nonbasic variable has improving reduced cost.
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
 * @return CXF_OK if transitioned to Phase II,
 *         CXF_INFEASIBLE if truly infeasible,
 *         1 if has improving direction (continue Phase I)
 */
int cxf_check_phase_one_end(SolverState *state, CxfModel *model, CxfEnv *env) {
    double tol = env->feasibility_tol;

    /* Compute Phase I objective fresh to guard against drift */
    double fresh_obj = compute_phase1_objective(state);
    double phase1_obj = (fresh_obj > state->obj_value)
                        ? fresh_obj : state->obj_value;
    state->obj_value = fresh_obj;

    if (phase1_obj <= tol) {
        /* Phase I objective is zero — feasible, transition to Phase II */
        int rc = cxf_transition_to_phase_two(state, model);
        if (rc != CXF_OK) return rc;
        cxf_compute_reduced_costs(state);
        return CXF_OK;
    }

    /* Phase I optimal but objective still positive.
     * Safety net: force refactorization + recompute to clear drift,
     * then scan for improving directions. */
    {
        cxf_solver_refactor(state, env);
        cxf_recompute_xB(state);
        cxf_recompute_objective(state);
        /* Re-check: recomputation may have made us feasible */
        if (state->obj_value <= tol) {
            int rc2 = cxf_transition_to_phase_two(state, model);
            if (rc2 != CXF_OK) return rc2;
            cxf_compute_reduced_costs(state);
            return CXF_OK;
        }
    }
    cxf_compute_reduced_costs(state);
    if (has_improving_direction(state, env)) return 1;

    /* Phase I near-feasibility: try tighter pricing tolerance.
     * Spec: two_phase_method.md line 142 — "solver may attempt
     * additional iterations with tighter tolerances."
     * IMPORTANT: restore tolerance before returning — leaking the
     * tighter value corrupts all subsequent Phase I/II pricing. */
    if (phase1_obj < 100.0 * tol) {
        double save_tol = env->optimality_tol;
        env->optimality_tol *= 0.01;
        cxf_compute_reduced_costs(state);
        int found = has_improving_direction(state, env);
        env->optimality_tol = save_tol;  /* always restore */
        if (found) return 1;
    }

    /* No improving direction → truly infeasible */
    return CXF_INFEASIBLE;
}
