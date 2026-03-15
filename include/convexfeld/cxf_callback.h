/**
 * @file cxf_callback.h
 * @brief CallbackContext structure - user callback state.
 *
 * Manages user-defined callback functions during optimization.
 * Tracks callback registration, timing, and execution context.
 */

#ifndef CXF_CALLBACK_H
#define CXF_CALLBACK_H

#include "cxf_types.h"

/**
 * @brief Callback invocation context constants.
 *
 * WHERE codes indicate when the callback is being invoked during optimization.
 */
#define CXF_CB_PRE_SOLVE    1  /**< Before optimization begins */
#define CXF_CB_POLLING      2  /**< During optimization (polling) */
#define CXF_CB_MIP_SOL      3  /**< MIP solution found */
#define CXF_CB_POST_SOLVE   4  /**< After optimization completes */

/* CxfCallbackFunc typedef is in cxf_types.h (V2: resides on Environment) */

/**
 * @brief Callback context structure.
 *
 * Tracks callback registration and execution state.
 * Provides bridge between solver and user application.
 */
struct CallbackContext {
    uint32_t magic;           /**< Validation magic (CXF_CALLBACK_MAGIC) */
    uint64_t safety_magic;    /**< Safety magic (CXF_CALLBACK_MAGIC2) */

    /* Callback registration — per V2 spec (callback_state.md), the callback
     * function pointer resides on the Environment, not CallbackState.
     * See CxfEnv.callback_func. */
    void *user_data;          /**< User-provided data pointer */

    /* State — per V2 spec, termination uses Environment's asyncState
     * (terminate_flag). See CxfEnv.terminate_flag. */
    int enabled;              /**< 1 if callback enabled */

    /* Timing */
    double start_time;        /**< Callback session start time */
    int iteration_count;      /**< Current iteration count (non-spec, operational) */
    double best_obj;          /**< Best objective found (non-spec, operational) */

    /* Statistics */
    double callback_calls;    /**< Cumulative callback invocations */
    double callback_time;     /**< Cumulative time in callbacks (seconds) */
};

/*******************************************************************************
 * CallbackContext Lifecycle Functions (M5.2.2)
 ******************************************************************************/

/**
 * @brief Create and initialize a CallbackContext.
 *
 * Allocates a new CallbackContext with magic numbers set and all
 * fields initialized to default values. The callback is disabled
 * initially (enabled=0).
 *
 * @return Pointer to new CallbackContext, or NULL on allocation failure.
 */
CallbackContext *cxf_callback_create(void);

/**
 * @brief Free a CallbackContext.
 *
 * Deallocates the CallbackContext. Safe to call with NULL.
 *
 * @param ctx CallbackContext to free (may be NULL).
 */
void cxf_callback_free(CallbackContext *ctx);

/**
 * @brief Validate a CallbackContext.
 *
 * Checks that magic numbers are valid and structure is usable.
 *
 * @param ctx CallbackContext to validate.
 * @return CXF_OK if valid, error code otherwise.
 */
int cxf_callback_validate(const CallbackContext *ctx);

/**
 * @brief Reset CallbackContext statistics.
 *
 * Clears callback_calls, callback_time, and iteration_count.
 * Does not change user_data or enabled state.
 *
 * @param ctx CallbackContext to reset.
 * @return CXF_OK on success, error code if ctx is NULL or invalid.
 */
int cxf_callback_reset_stats(CallbackContext *ctx);

#endif /* CXF_CALLBACK_H */
