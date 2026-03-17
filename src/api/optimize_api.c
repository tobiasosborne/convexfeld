/**
 * @file optimize_api.c
 * @brief Optimize API implementation (M8.1.14)
 *
 * Implements internal optimization dispatcher and termination control.
 * This module provides the internal optimization entry point that bridges
 * the public API (cxf_optimize) and the core solver (cxf_solve_lp).
 *
 * V2 parameter_system.md: backs up all user-settable parameters before
 * dispatch and restores them unconditionally afterward, so solver-internal
 * modifications never leak into the user's parameter state.
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_callback.h"
#include <stdint.h>

#include "../simplex/simplex_internal.h"

/*******************************************************************************
 * Parameter backup/restore (V2 parameter_system.md Phase 5)
 *
 * All user-settable parameters on CxfEnv are captured before solver dispatch
 * and restored afterward, regardless of success or failure.
 ******************************************************************************/

/** Snapshot of every user-settable parameter on CxfEnv. */
typedef struct {
    /* Tolerances */
    double feasibility_tol;
    double optimality_tol;
    double infinity;
    /* Logging */
    int output_flag;
    int verbosity;
    /* Algorithm selection */
    int method;
    /* Threading */
    int threads;
    int license_thread_limit;
    /* Refactorization */
    int max_eta_count;
    int64_t max_eta_memory;
    int refactor_interval;
} ParamBackup;

/** Save all user-settable parameters from env into backup. */
static void backup_params(const CxfEnv *env, ParamBackup *bk) {
    bk->feasibility_tol     = env->feasibility_tol;
    bk->optimality_tol      = env->optimality_tol;
    bk->infinity            = env->infinity;
    bk->output_flag         = env->output_flag;
    bk->verbosity           = env->verbosity;
    bk->method              = env->method;
    bk->threads             = env->threads;
    bk->license_thread_limit = env->license_thread_limit;
    bk->max_eta_count       = env->max_eta_count;
    bk->max_eta_memory      = env->max_eta_memory;
    bk->refactor_interval   = env->refactor_interval;
}

/** Restore all user-settable parameters from backup into env. */
static void restore_params(CxfEnv *env, const ParamBackup *bk) {
    env->feasibility_tol     = bk->feasibility_tol;
    env->optimality_tol      = bk->optimality_tol;
    env->infinity            = bk->infinity;
    env->output_flag         = bk->output_flag;
    env->verbosity           = bk->verbosity;
    env->method              = bk->method;
    env->threads             = bk->threads;
    env->license_thread_limit = bk->license_thread_limit;
    env->max_eta_count       = bk->max_eta_count;
    env->max_eta_memory      = bk->max_eta_memory;
    env->refactor_interval   = bk->refactor_interval;
}

/**
 * @brief Internal optimization dispatcher.
 *
 * Called by cxf_optimize after initial validation. Backs up all user-settable
 * parameters before dispatch and restores them unconditionally afterward
 * (V2 parameter_system.md Phase 5, Restore-on-Error Guarantee).
 *
 * @param model Model to optimize (must be non-NULL and valid)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize_internal(CxfModel *model) {
    int status;
    CxfEnv *env;
    ParamBackup saved;

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

    /* V2 parameter_system.md Phase 5: backup parameters before dispatch */
    backup_params(env, &saved);

    /* Log optimization start */
    cxf_log_printf(env, 0, "Starting optimization for model '%s'", model->name);
    cxf_log_printf(env, 0, "Model: %d variables, %d constraints",
                   model->num_vars, model->num_constrs);

    /* Set self-pointer for optimization session tracking */
    model->self_ptr = model;

    /* Clear model status code per solve_entry.md Step 4 */
    model->status = CXF_LOADED;

    /* Reset termination flag at start of optimization */
    cxf_reset_terminate(env);

    /* Mark optimization as in progress */
    env->optimizing = 1;

    /* Pre-optimization callback */
    status = cxf_pre_optimize_hook(model);
    if (status != 0) {
        cxf_log_printf(env, 0, "Pre-optimization callback requested termination");
        env->optimizing = 0;
        restore_params(env, &saved);
        return CXF_ERROR_INVALID_ARGUMENT;  /* Callback requested abort */
    }

    /* Method dispatch per V2 parameters_defaults.md §3:
     * -1=auto, 0=primal simplex, 1-5=not yet implemented */
    {
        int method = env->method;
        if (method == -1 || method == 0) {
            /* Auto or primal simplex: delegate to LP solver */
            status = cxf_solve_lp(model);
        } else {
            /* Methods 1-5 (dual, barrier, concurrent, det-concurrent, PDHG)
             * are not yet implemented */
            cxf_log_printf(env, 0,
                "Method %d is not supported; only primal simplex "
                "(Method=0) and automatic (Method=-1) are available", method);
            env->optimizing = 0;
            restore_params(env, &saved);
            return CXF_ERROR_NOT_SUPPORTED;
        }
    }

    /* Post-optimization callback */
    (void)cxf_post_optimize_hook(model);

    /* Log optimization completion */
    if (status == CXF_OPTIMAL) {
        cxf_log_printf(env, 0, "Optimization completed successfully");
        cxf_log_printf(env, 0, "Objective value: %.6f", model->obj_val);
    } else {
        cxf_log_printf(env, 0, "Optimization completed with status: %d", status);
    }

    /* Clear optimization flag */
    env->optimizing = 0;

    /* V2 parameter_system.md Phase 5: restore parameters after dispatch */
    restore_params(env, &saved);

    /* V2 solve_entry.md: return 0 on success, non-zero only for errors.
     * Solver outcome codes (OPTIMAL, INFEASIBLE, UNBOUNDED, ITERATION_LIMIT,
     * TIME_LIMIT, etc.) are stored on model->status and are NOT errors.
     * Only error codes (10001+) propagate as return values. */
    if (CXF_IS_ERROR(status)) {
        return status;
    }
    return CXF_OK;
}
