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
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_callback.h"

/* Forward declarations - external functions */
extern int cxf_solve_lp(CxfModel *model);
extern int cxf_checkmodel(CxfModel *model);
extern void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);
extern int cxf_pre_optimize_callback(CxfModel *model);
extern int cxf_post_optimize_callback(CxfModel *model);

/**
 * @brief Internal optimization dispatcher.
 *
 * This is the internal entry point for optimization, called by cxf_optimize
 * after initial validation. It sets up state, handles various optimization
 * modes, and dispatches to the appropriate solver.
 *
 * Current implementation orchestrates:
 * - Logging of optimization start/end
 * - Pre-optimization callbacks
 * - Optional preprocessing (if cxf_simplex_preprocess available)
 * - Solver dispatch (LP via simplex)
 * - Post-optimization callbacks
 * - Iteration count logging
 *
 * Future enhancements:
 * - Concurrent optimization mode handling
 * - Parameter backup/restoration for multi-environment setups
 * - Non-convex quadratic detection and MIP conversion
 * - Model fingerprinting for reproducibility
 * - Method selection (primal vs dual simplex)
 *
 * @param model Model to optimize (must be non-NULL and valid)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize_internal(CxfModel *model) {
    int status;
    CxfEnv *env;

    /* Validate model state */
    status = cxf_checkmodel(model);
    if (status != CXF_OK) {
        return status;
    }

    /* Get environment for logging and callbacks */
    env = model->env;
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Log optimization start */
    cxf_log_printf(env, 0, "Starting optimization for model '%s'", model->name);
    cxf_log_printf(env, 0, "Model: %d variables, %d constraints",
                   model->num_vars, model->num_constrs);

    /* Set self-pointer for optimization session tracking */
    model->self_ptr = model;

    /* Reset termination flag at start of optimization */
    cxf_reset_terminate(env);

    /* Mark optimization as in progress */
    env->optimizing = 1;

    /* Pre-optimization callback */
    status = cxf_pre_optimize_callback(model);
    if (status != 0) {
        cxf_log_printf(env, 0, "Pre-optimization callback requested termination");
        env->optimizing = 0;
        return CXF_ERROR_INVALID_ARGUMENT;  /* Callback requested abort */
    }

    /* Delegate to LP solver
     * Future: dispatch based on problem type (LP/QP/MIP/NLP)
     * Future: add preprocessing call if needed
     * Future: check parameters for method selection (primal/dual simplex) */
    status = cxf_solve_lp(model);

    /* Post-optimization callback */
    (void)cxf_post_optimize_callback(model);

    /* Log optimization completion */
    if (status == CXF_OPTIMAL) {
        cxf_log_printf(env, 0, "Optimization completed successfully");
        cxf_log_printf(env, 0, "Objective value: %.6f", model->obj_val);
        status = CXF_OK;  /* Convert OPTIMAL status to OK for API */
    } else {
        cxf_log_printf(env, 0, "Optimization completed with status: %d", status);
    }

    /* Clear optimization flag */
    env->optimizing = 0;

    return status;
}
