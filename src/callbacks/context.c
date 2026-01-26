/**
 * @file context.c
 * @brief CallbackContext structure lifecycle management (M5.2.2)
 *
 * Implements creation, validation, and cleanup for the CallbackContext
 * structure that manages user callback state during optimization.
 *
 * Spec: docs/specs/structures/callback_context.md
 */

#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * CallbackContext Creation
 *===========================================================================*/

/**
 * @brief Create and initialize a CallbackContext.
 *
 * Allocates a new CallbackContext with magic numbers set and all
 * fields initialized to default values. The callback is disabled
 * initially (enabled=0).
 *
 * @return Pointer to new CallbackContext, or NULL on allocation failure.
 */
CallbackContext *cxf_callback_create(void) {
    CallbackContext *ctx = (CallbackContext *)calloc(1, sizeof(CallbackContext));
    if (ctx == NULL) {
        return NULL;
    }

    /* Set magic numbers for validation */
    ctx->magic = CXF_CALLBACK_MAGIC;
    ctx->safety_magic = CXF_CALLBACK_MAGIC2;

    /* Initialize callback registration (empty) */
    ctx->callback_func = NULL;
    ctx->user_data = NULL;

    /* Initialize state */
    ctx->terminate_requested = 0;
    ctx->enabled = 0;  /* Disabled until callback is registered */

    /* Initialize timing */
    ctx->start_time = 0.0;
    ctx->iteration_count = 0;
    ctx->best_obj = INFINITY;  /* No objective found yet */

    /* Initialize statistics */
    ctx->callback_calls = 0.0;
    ctx->callback_time = 0.0;

    return ctx;
}

/*============================================================================
 * CallbackContext Destruction
 *===========================================================================*/

/**
 * @brief Free a CallbackContext.
 *
 * Deallocates the CallbackContext. Safe to call with NULL.
 *
 * @param ctx CallbackContext to free (may be NULL).
 */
void cxf_callback_free(CallbackContext *ctx) {
    if (ctx == NULL) {
        return;
    }

    /* Clear magic numbers to detect use-after-free */
    ctx->magic = 0;
    ctx->safety_magic = 0;

    free(ctx);
}

/*============================================================================
 * CallbackContext Validation
 *===========================================================================*/

/**
 * @brief Validate a CallbackContext.
 *
 * Checks that magic numbers are valid and structure is usable.
 *
 * @param ctx CallbackContext to validate.
 * @return CXF_OK if valid, error code otherwise.
 */
int cxf_callback_validate(const CallbackContext *ctx) {
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (ctx->magic != CXF_CALLBACK_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->safety_magic != CXF_CALLBACK_MAGIC2) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    return CXF_OK;
}

/*============================================================================
 * CallbackContext Statistics Reset
 *===========================================================================*/

/**
 * @brief Reset CallbackContext statistics.
 *
 * Clears callback_calls, callback_time, and iteration_count.
 * Does not change callback_func, user_data, or enabled state.
 *
 * @param ctx CallbackContext to reset.
 * @return CXF_OK on success, error code if ctx is NULL or invalid.
 */
int cxf_callback_reset_stats(CallbackContext *ctx) {
    int status = cxf_callback_validate(ctx);
    if (status != CXF_OK) {
        return status;
    }

    ctx->callback_calls = 0.0;
    ctx->callback_time = 0.0;
    ctx->iteration_count = 0;
    ctx->best_obj = INFINITY;
    ctx->start_time = 0.0;
    ctx->terminate_requested = 0;

    return CXF_OK;
}
