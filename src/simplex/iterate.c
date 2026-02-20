/**
 * @file iterate.c
 * @brief Iteration progress logging (v2 P3.20 — thin wrapper)
 *
 * Currently delegates to cxf_simplex_step() in step.c.
 * A2 (convexfeld-qh9y) will convert this to logging-only.
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

/* The real iteration engine is now in step.c */
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);

/**
 * @brief Perform one simplex iteration (thin wrapper).
 *
 * All iteration logic has been moved to cxf_simplex_step() in step.c.
 * This wrapper preserves the existing call interface for callers
 * (solve_lp.c, phase_loop.c) until A2 converts them.
 */
int cxf_log_iteration_progress(SolverState *state, CxfEnv *env) {
    return cxf_simplex_step(state, env);
}
