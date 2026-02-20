/**
 * @file iterate.c
 * @brief Iteration progress logging (v2 P3.20)
 *
 * Reports presolve/iteration progress to the log and invokes
 * the external monitoring callback. No iteration logic — that
 * lives in cxf_simplex_step() in step.c.
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Report iteration progress (v2 logging-only).
 *
 * V2 spec signature: (CxfModel *model, SolverState *state) -> void.
 * Steps:
 *   1. If logging disabled on model, skip to step 4.
 *   2. Time throttle: at most one message per normalized second.
 *   3. Format and emit progress message.
 *   4. Invoke external monitoring callback unconditionally.
 *
 * Currently a no-op stub. Full implementation requires:
 *   - Logging infrastructure (convexfeld-1lkf)
 *   - Callback system wiring
 *
 * @param model  Model with logging config and thread count
 * @param state  Solver state with timing and progress data
 */
void cxf_log_iteration_progress(CxfModel *model, SolverState *state) {
    (void)model;
    (void)state;
    /* TODO: Implement when logging infrastructure (convexfeld-1lkf) lands */
}
