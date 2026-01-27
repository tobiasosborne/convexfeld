/**
 * @file terminate.c
 * @brief Termination handling functions (M5.2.5)
 *
 * Thread-safe termination signaling for optimization loops and callbacks.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"

/*============================================================================
 * Environment Termination
 *===========================================================================*/

/**
 * @brief Set the termination flag for an environment.
 *
 * Sets both the environment's internal terminate_flag and the external
 * terminate_flag_ptr (if configured). This provides thread-safe termination
 * signaling for optimization loops.
 *
 * Safe to call with NULL environment (no-op).
 *
 * @param env Environment to terminate (may be NULL)
 */
void cxf_set_terminate(CxfEnv *env) {
    if (env == NULL) {
        return;
    }

    /* Set internal termination flag */
    env->terminate_flag = 1;

    /* Set external termination flag if configured */
    if (env->terminate_flag_ptr != NULL) {
        *env->terminate_flag_ptr = 1;
    }

    /* Note: AsyncState not implemented yet, so we skip that */
}

/*============================================================================
 * Model/Callback Termination
 *===========================================================================*/

/**
 * @brief Request termination from within a callback.
 *
 * Sets termination flags in the model's environment and callback state.
 * This provides a callback-safe way for user code to signal termination
 * during optimization.
 *
 * Safe to call with NULL model or NULL environment (no-op).
 *
 * @param model Model being optimized (may be NULL)
 */
void cxf_callback_terminate(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return;
    }

    /* Set environment termination flag */
    model->env->terminate_flag = 1;

    /* Set callback-specific termination flag if callback state exists */
    if (model->env->callback_state != NULL) {
        model->env->callback_state->terminate_requested = 1;
    }

    /* Set external termination flag if configured */
    if (model->env->terminate_flag_ptr != NULL) {
        *model->env->terminate_flag_ptr = 1;
    }
}
