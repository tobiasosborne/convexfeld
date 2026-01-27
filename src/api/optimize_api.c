/**
 * @file optimize_api.c
 * @brief Optimize API implementation (M8.1.14)
 *
 * Implements internal optimization dispatcher and termination control.
 * This module provides the internal optimization entry point that bridges
 * the public API (cxf_optimize) and the core solver (cxf_solve_lp).
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations - external functions */
extern int cxf_solve_lp(CxfModel *model);
extern int cxf_checkmodel(CxfModel *model);

/**
 * @brief Internal optimization dispatcher.
 *
 * This is the internal entry point for optimization, called by cxf_optimize
 * after initial validation. It sets up state, handles various optimization
 * modes, and dispatches to the appropriate solver.
 *
 * Current implementation is a minimal stub that validates the model and
 * delegates to cxf_solve_lp for LP problems. Future enhancements will add:
 * - Concurrent optimization mode handling
 * - Parameter backup/restoration for multi-environment setups
 * - Non-convex quadratic detection and MIP conversion
 * - Model fingerprinting for reproducibility
 * - Presolve setup and dispatch
 *
 * @param model Model to optimize (must be non-NULL and valid)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize_internal(CxfModel *model) {
    int status;

    /* Validate model state */
    status = cxf_checkmodel(model);
    if (status != CXF_OK) {
        return status;
    }

    /* Set self-pointer for optimization session tracking */
    model->self_ptr = model;

    /* Reset termination flag at start of optimization */
    if (model->env != NULL) {
        cxf_reset_terminate(model->env);
    }

    /* Mark optimization as in progress */
    if (model->env != NULL) {
        model->env->optimizing = 1;
    }

    /* Delegate to LP solver
     * Future: dispatch based on problem type (LP/QP/MIP/NLP) */
    status = cxf_solve_lp(model);

    /* Clear optimization flag */
    if (model->env != NULL) {
        model->env->optimizing = 0;
    }

    return status;
}
