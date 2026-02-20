/**
 * @file phase_steps.c
 * @brief Bound propagation stubs (v2 P3.20)
 *
 * cxf_simplex_step2: Variable-side bound propagation (stub)
 * cxf_simplex_step3: Constraint-side bound propagation (stub)
 *
 * Full implementations in E2 (convexfeld-reb8) and E3 (convexfeld-dz1w).
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Variable-side bound propagation (v2 stub).
 *
 * Processes the variable-side bound-flipping queue populated
 * during cxf_simplex_step. Currently a no-op.
 *
 * @param state Solver state
 * @param env   Environment with tolerances
 * @return 0 on success
 */
int cxf_simplex_step2(SolverState *state, CxfEnv *env) {
    (void)state;
    (void)env;
    return 0;
}

/**
 * @brief Constraint-side bound propagation (v2 stub).
 *
 * Computes implied variable bounds from constraint activities.
 * LP only (skipped for QP). Currently a no-op.
 *
 * @param state Solver state
 * @param env   Environment with tolerances
 * @return 0 on success
 */
int cxf_simplex_step3(SolverState *state, CxfEnv *env) {
    (void)state;
    (void)env;
    return 0;
}
