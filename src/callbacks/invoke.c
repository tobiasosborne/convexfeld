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
 * - docs/specs/functions/callbacks/cxf_pre_optimize_hook.md
 * - docs/specs/functions/callbacks/cxf_post_optimize_hook.md
 */

#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_timing.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/**
 * @brief Invoke a user callback at a given optimization phase.
 *
 * Shared implementation for pre- and post-optimization hooks.
 * Performs guard checks, timing, invocation counting, and optionally
 * sets the termination flag on non-zero return.
 *
 * @param model      Model being optimized.
 * @param where      CXF_CB_PRE_SOLVE or CXF_CB_POST_SOLVE.
 * @param set_terminate  If true, set env->terminate_flag on non-zero return.
 * @return 0 to continue, non-zero if callback requests abort.
 */
static int invoke_callback_hook(CxfModel *model, int where,
                                int set_terminate) {
    if (model == NULL) return 0;

    CxfEnv *env = model->env;
    if (env == NULL) return 0;

    CallbackContext *ctx = env->callback_state;
    if (ctx == NULL) return 0;

    if (ctx->enabled == 0) return 0;

    /* V2: callback_func resides on Environment, not CallbackState */
    CxfCallbackFunc callback_func = env->callback_func;
    if (callback_func == NULL) return 0;

    void *user_data = ctx->user_data;
    void *cbdata = (void *)ctx;

    ctx->callback_calls += 1.0;

    double start_time = cxf_get_elapsed_time();
    int result = callback_func(model, cbdata, where, user_data);
    double end_time = cxf_get_elapsed_time();
    ctx->callback_time += end_time - start_time;

    /* V2: termination via Environment's asyncState (terminate_flag) */
    if (result != 0 && set_terminate) {
        env->terminate_flag = 1;
    }

    return result;
}

/**
 * @brief Invoke user callback before optimization begins.
 *
 * @param model Model being optimized
 * @return 0 to continue optimization, non-zero to abort
 */
int cxf_pre_optimize_hook(CxfModel *model) {
    return invoke_callback_hook(model, CXF_CB_PRE_SOLVE, 1);
}

/**
 * @brief Invoke user callback after optimization completes.
 *
 * @param model Model that was optimized
 * @return 0 on success, non-zero if callback encountered error
 */
int cxf_post_optimize_hook(CxfModel *model) {
    return invoke_callback_hook(model, CXF_CB_POST_SOLVE, 0);
}
