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
 * @brief Callback function signature.
 * @param model The model being optimized
 * @param cbdata User callback data
 * @return 0 to continue, non-zero to terminate
 */
typedef int (*CxfCallbackFunc)(CxfModel *model, void *cbdata);

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

#endif /* CXF_CALLBACK_H */
