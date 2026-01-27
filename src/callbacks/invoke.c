/**
 * @file invoke.c
 * @brief Callback invocation functions (M5.2.4)
 *
 * Implements pre- and post-optimization callback invocation with timing
 * and statistics tracking. These functions serve as wrappers around user
 * callbacks, handling infrastructure concerns (timing, invocation counting,
 * termination signaling) while delegating domain logic to user code.
 *
 * Specs:
 * - docs/specs/functions/callbacks/cxf_pre_optimize_callback.md
 * - docs/specs/functions/callbacks/cxf_post_optimize_callback.md
 */

#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_timing.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/*============================================================================
 * Pre-Optimization Callback Invocation
 *===========================================================================*/

/**
 * @brief Invoke user callback before optimization begins.
 *
 * Called immediately before optimization starts. Allows user to inspect
 * initial model state, modify parameters, perform validation, or abort
 * optimization by returning non-zero.
 *
 * Guard-check pattern ensures safety when callback infrastructure is
 * missing or disabled. Tracks timing and invocation statistics. Sets
 * termination flag if callback requests abort.
 *
 * @param model Model being optimized
 * @return 0 to continue optimization, non-zero to abort
 *
 * @note Returns 0 (success) if callback infrastructure is missing/disabled
 * @note Environment lock must be held by caller
 */
int cxf_pre_optimize_callback(CxfModel *model) {
    /* Guard: Check model exists */
    if (model == NULL) {
        return 0;
    }

    /* Guard: Get environment from model */
    CxfEnv *env = model->env;
    if (env == NULL) {
        return 0;
    }

    /* Guard: Get callback context from environment */
    CallbackContext *ctx = env->callback_state;
    if (ctx == NULL) {
        return 0;
    }

    /* Guard: Check if callback is enabled */
    if (ctx->enabled == 0) {
        return 0;
    }

    /* Guard: Get callback function pointer */
    CxfCallbackFunc callback_func = ctx->callback_func;
    if (callback_func == NULL) {
        return 0;
    }

    /* Retrieve user data */
    void *user_data = ctx->user_data;

    /* Increment invocation counter */
    ctx->callback_calls += 1.0;

    /* Capture start timestamp */
    double start_time = cxf_get_timestamp();

    /* Invoke user callback */
    int result = callback_func(model, user_data);

    /* Capture end timestamp and update cumulative time */
    double end_time = cxf_get_timestamp();
    double elapsed = end_time - start_time;
    ctx->callback_time += elapsed;

    /* If callback returned non-zero, set termination flag */
    if (result != 0) {
        ctx->terminate_requested = 1;
    }

    return result;
}

/*============================================================================
 * Post-Optimization Callback Invocation
 *===========================================================================*/

/**
 * @brief Invoke user callback after optimization completes.
 *
 * Called immediately after optimization finishes. Allows user to inspect
 * final solution and statistics, log results, perform post-processing, or
 * trigger follow-up actions.
 *
 * Nearly identical to pre-optimization callback but differs in semantic
 * context: model now contains final solution data. Return value does not
 * affect optimization (already complete) but may be logged for diagnostics.
 *
 * @param model Model that was optimized
 * @return 0 on success, non-zero if callback encountered error
 *
 * @note Returns 0 (success) if callback infrastructure is missing/disabled
 * @note Environment lock must be held by caller
 * @note Does NOT set termination flag (optimization already complete)
 */
int cxf_post_optimize_callback(CxfModel *model) {
    /* Guard: Check model exists */
    if (model == NULL) {
        return 0;
    }

    /* Guard: Get environment from model */
    CxfEnv *env = model->env;
    if (env == NULL) {
        return 0;
    }

    /* Guard: Get callback context from environment */
    CallbackContext *ctx = env->callback_state;
    if (ctx == NULL) {
        return 0;
    }

    /* Guard: Check if callback is enabled */
    if (ctx->enabled == 0) {
        return 0;
    }

    /* Guard: Get callback function pointer */
    CxfCallbackFunc callback_func = ctx->callback_func;
    if (callback_func == NULL) {
        return 0;
    }

    /* Retrieve user data */
    void *user_data = ctx->user_data;

    /* Increment invocation counter */
    ctx->callback_calls += 1.0;

    /* Capture start timestamp */
    double start_time = cxf_get_timestamp();

    /* Invoke user callback */
    int result = callback_func(model, user_data);

    /* Capture end timestamp and update cumulative time */
    double end_time = cxf_get_timestamp();
    double elapsed = end_time - start_time;
    ctx->callback_time += elapsed;

    /* Note: Do NOT set termination flag - optimization is complete */

    return result;
}
