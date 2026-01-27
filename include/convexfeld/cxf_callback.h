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

/**
 * @brief Callback function signature.
 * @param model The model being optimized
 * @param cbdata Callback data pointer (typically CallbackContext)
 * @param where Context code indicating invocation point (CXF_CB_*)
 * @param usrdata User-provided data pointer
 * @return 0 to continue, non-zero to terminate
 */
typedef int (*CxfCallbackFunc)(CxfModel *model, void *cbdata, int where, void *usrdata);

/**
 * @brief Callback context structure.
 *
 * Tracks callback registration and execution state.
 * Provides bridge between solver and user application.
 */
struct CallbackContext {
    uint32_t magic;           /**< Validation magic (CXF_CALLBACK_MAGIC) */
    uint64_t safety_magic;    /**< Safety magic (CXF_CALLBACK_MAGIC2) */

    /* Callback registration */
    CxfCallbackFunc callback_func; /**< User callback function */
    void *user_data;          /**< User-provided data pointer */

    /* State */
    int terminate_requested;  /**< 1 if termination requested */
    int enabled;              /**< 1 if callback enabled */

    /* Timing */
    double start_time;        /**< Callback session start time */
    int iteration_count;      /**< Current iteration count */
    double best_obj;          /**< Best objective found */

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
 * Does not change callback_func, user_data, or enabled state.
 *
 * @param ctx CallbackContext to reset.
 * @return CXF_OK on success, error code if ctx is NULL or invalid.
 */
int cxf_callback_reset_stats(CallbackContext *ctx);

#endif /* CXF_CALLBACK_H */
