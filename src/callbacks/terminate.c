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
 * @brief Request termination from within a callback (V2 spec).
 *
 * Traverses to the root environment (via master_env chain) and sets the
 * termination flag there. V2: termination uses Environment's asyncState
 * (terminate_flag), not CallbackContext. Also sets external
 * terminate_flag_ptr if configured.
 *
 * Per V2 spec: returns 0 on success, error code on failure.
 * Null model or null env returns CXF_ERROR_NULL_ARGUMENT.
 * Null async state (terminate_flag_ptr) is silently skipped.
 *
 * @param model Model being optimized
 * @return 0 on success, error code on failure
 */
int cxf_callback_terminate(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Traverse to root environment per V2 spec */
    CxfEnv *root = model->env;
    while (root->master_env != NULL) {
        root = root->master_env;
    }

    /* Set termination flag on root environment */
    root->terminate_flag = 1;

    /* Set external termination flag if configured (silently skip if NULL) */
    if (root->terminate_flag_ptr != NULL) {
        *root->terminate_flag_ptr = 1;
    }

    /* V2: termination via env->terminate_flag only, not CallbackContext */

    return CXF_OK;
}
