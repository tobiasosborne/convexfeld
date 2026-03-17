/**
 * @file init.c
 * @brief Callback initialization functions (M5.2.3)
 *
 * Implements callback initialization and reset functions:
 * - cxf_init_callback_struct: Allocate and init mutex per V2 callbacks.md
 * - cxf_reset_callback_state: Reset callback state counters
 *
 * Specs:
 * - docs/specs-v2/specs/modules/callbacks.md (cxf_init_callback_struct)
 * - docs/specs/functions/callbacks/cxf_reset_callback_state.md
 */

#define _POSIX_C_SOURCE 199309L

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_timing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

/* Internal memory functions */
void *cxf_malloc(size_t size);
void  cxf_free(void *ptr);

/*============================================================================
 * cxf_init_callback_struct
 *===========================================================================*/

/**
 * @brief Allocate and initialize a mutex for callback serialization.
 *
 * Per V2 callbacks.md: allocates a pthread_mutex_t, initializes it to
 * unlocked state, and writes the pointer to *mutex_out. On failure,
 * *mutex_out is set to NULL and OOM error code is returned.
 *
 * The env parameter is accepted for API consistency but not used.
 *
 * @param env  Environment (unused, may be NULL).
 * @param mutex_out  Output: receives allocated mutex pointer.
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if mutex_out is NULL,
 *         CXF_ERROR_OUT_OF_MEMORY on allocation failure.
 */
int cxf_init_callback_struct(CxfEnv *env, void **mutex_out) {
    pthread_mutex_t *mtx;
    (void)env;  /* Unused per spec — kept for API consistency */

    if (mutex_out == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* V2 spec: set output to NULL first (clean state even on failure) */
    *mutex_out = NULL;

    mtx = (pthread_mutex_t *)cxf_malloc(sizeof(pthread_mutex_t));
    if (mtx == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    pthread_mutex_init(mtx, NULL);
    *mutex_out = mtx;
    return CXF_OK;
}

/*============================================================================
 * cxf_reset_callback_state
 *===========================================================================*/

/**
 * @brief Reset callback state in environment.
 *
 * Resets callback state counters and temporary fields while preserving
 * the CallbackState allocation and user configuration (user_data,
 * enabled flag, magic numbers).
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
 *
 * Fields preserved:
 * - magic, safety_magic
 * - user_data (V2: callback_func is on CxfEnv)
 * - enabled
 * V2: terminate_requested removed; use env->terminate_flag
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
    current_time = cxf_get_elapsed_time();

    /* Reset statistics */
    ctx->callback_calls = 0.0;
    ctx->callback_time = 0.0;

    /* Reset iteration tracking */
    ctx->iteration_count = 0;
    ctx->best_obj = INFINITY;  /* No objective found yet */

    /* Reset timing - mark start of new run */
    ctx->start_time = current_time;

    /* V2: termination via env->terminate_flag, not ctx->terminate_requested.
     * Preserve: magic, safety_magic, user_data, enabled.
     * callback_func is on CxfEnv, not CallbackContext. */
}
