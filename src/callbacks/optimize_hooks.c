/**
 * @file optimize_hooks.c
 * @brief Optimization lifecycle hooks (P3.13)
 *
 * Implements cxf_pre_optimize_callback and cxf_post_optimize_callback
 * per V2 callbacks.md. These are internal lifecycle hooks, NOT user
 * callbacks. They manage the error buffer lock flag to implement
 * first-error preservation during optimization.
 *
 * Pattern:
 *   1. cxf_pre_optimize_callback sets error_buf_locked = 1
 *   2. Optimization runs (cascading errors preserve first message)
 *   3. cxf_post_optimize_callback clears error_buf_locked = 0
 *
 * Spec: docs/specs-v2/specs/modules/callbacks.md
 *       (cxf_pre_optimize_callback, cxf_post_optimize_callback)
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Lock the error buffer before optimization begins.
 *
 * Per V2 callbacks.md: validates the model using structural validation
 * (sentinel-based). If valid, sets error_buf_locked = 1 on the model's
 * environment to preserve any pre-existing error messages during the
 * solve. If the model is invalid or NULL, returns silently.
 *
 * This is an internal lifecycle hook, NOT a user callback.
 *
 * @param model  The model about to be optimized (may be NULL).
 */
void cxf_pre_optimize_callback(CxfModel *model) {
    if (model == NULL) {
        return;
    }

    /* Structural validation (sentinel-based) */
    if (cxf_checkmodel(model) != CXF_OK) {
        return;
    }

    CxfEnv *env = model->env;
    if (env == NULL) {
        return;
    }

    env->error_buf_locked = 1;
}

/**
 * @brief Unlock the error buffer after optimization completes.
 *
 * Per V2 callbacks.md: validates the model using structural validation
 * (sentinel-based). If valid, clears error_buf_locked = 0 on the
 * model's environment, restoring normal error reporting behavior.
 * If the model is invalid or NULL, returns silently.
 *
 * This function is idempotent: clearing an already-cleared lock has
 * no ill effects.
 *
 * This is an internal lifecycle hook, NOT a user callback.
 *
 * @param model  The model that was just optimized (may be NULL).
 */
void cxf_post_optimize_callback(CxfModel *model) {
    if (model == NULL) {
        return;
    }

    /* Structural validation (sentinel-based) */
    if (cxf_checkmodel(model) != CXF_OK) {
        return;
    }

    CxfEnv *env = model->env;
    if (env == NULL) {
        return;
    }

    env->error_buf_locked = 0;
}
