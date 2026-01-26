/**
 * @file init.c
 * @brief Callback initialization functions (M5.2.3)
 *
 * Implements callback initialization and reset functions:
 * - cxf_init_callback_struct: Initialize 48-byte callback substructure
 * - cxf_reset_callback_state: Reset callback state counters
 *
 * Specs:
 * - docs/specs/functions/callbacks/cxf_init_callback_struct.md
 * - docs/specs/functions/callbacks/cxf_reset_callback_state.md
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_timing.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * cxf_init_callback_struct
 *===========================================================================*/

/**
 * @brief Initialize a 48-byte callback sub-structure.
 *
 * Zeros the memory region pointed to by callbackSubStruct. This function
 * is called during CallbackState allocation to ensure clean initial state
 * before the caller sets specific fields.
 *
 * The env parameter is unused but kept for API consistency and future
 * extensibility.
 *
 * @param env Environment (unused, may be NULL).
 * @param callbackSubStruct Pointer to 48-byte memory region.
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if callbackSubStruct is NULL.
 */
int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct) {
    (void)env;  /* Unused per spec - kept for future extensibility */

    if (callbackSubStruct == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    memset(callbackSubStruct, 0, 48);
    return CXF_OK;
}

/*============================================================================
 * cxf_reset_callback_state
 *===========================================================================*/

/**
 * @brief Reset callback state in environment.
 *
 * Resets callback state counters and temporary fields while preserving
 * the CallbackState allocation and user configuration (callback_func,
 * user_data, enabled flag, magic numbers).
 *
 * This allows callback infrastructure to be reused across multiple
 * optimization runs without deallocation overhead.
 *
 * Fields reset:
 * - callback_calls -> 0.0
 * - callback_time -> 0.0
 * - iteration_count -> 0
 * - best_obj -> INFINITY
 * - start_time -> current timestamp
 * - terminate_requested -> 0
 *
 * Fields preserved:
 * - magic, safety_magic
 * - callback_func, user_data
 * - enabled
 *
 * NULL-safe: returns immediately if env or callback_state is NULL.
 *
 * @param env Environment whose callback state should be reset.
 */
void cxf_reset_callback_state(CxfEnv *env) {
    CallbackContext *ctx;
    double current_time;

    /* Defensive NULL check - silent return per spec */
    if (env == NULL) {
        return;
    }

    /* Get callback state from environment */
    ctx = env->callback_state;
    if (ctx == NULL) {
        return;  /* No callbacks registered - nothing to reset */
    }

    /* Get current timestamp for timing fields */
    current_time = cxf_get_timestamp();

    /* Reset statistics */
    ctx->callback_calls = 0.0;
    ctx->callback_time = 0.0;

    /* Reset iteration tracking */
    ctx->iteration_count = 0;
    ctx->best_obj = INFINITY;  /* No objective found yet */

    /* Reset timing - mark start of new run */
    ctx->start_time = current_time;

    /* Clear termination request from previous run */
    ctx->terminate_requested = 0;

    /* Preserve: magic, safety_magic, callback_func, user_data, enabled */
}
