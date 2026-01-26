/**
 * @file context.c
 * @brief PricingContext structure lifecycle management (M6.1.2)
 *
 * Implements multi-level partial pricing context for efficient
 * entering variable selection in the simplex method.
 */

#include <stdlib.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/*============================================================================
 * PricingContext Creation
 *===========================================================================*/

/**
 * @brief Create a new pricing context.
 *
 * Allocates and initializes a PricingContext with the specified number
 * of variables and pricing levels.
 *
 * @param num_vars Number of variables in the problem
 * @param max_levels Number of pricing levels (typically 3-5)
 * @return Newly allocated context, or NULL on failure
 */
PricingContext *cxf_pricing_create(int num_vars, int max_levels) {
    if (num_vars <= 0 || max_levels <= 0) {
        return NULL;
    }

    PricingContext *ctx = (PricingContext *)calloc(1, sizeof(PricingContext));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->max_levels = max_levels;
    ctx->current_level = 1;

    /* Allocate level arrays */
    ctx->candidate_counts = (int *)calloc((size_t)max_levels, sizeof(int));
    ctx->candidate_arrays = (int **)calloc((size_t)max_levels, sizeof(int *));
    ctx->cached_counts = (int *)calloc((size_t)max_levels, sizeof(int));

    if (ctx->candidate_counts == NULL || ctx->candidate_arrays == NULL ||
        ctx->cached_counts == NULL) {
        cxf_pricing_free(ctx);
        return NULL;
    }

    /* Initialize cached counts to -1 (invalid) */
    for (int i = 0; i < max_levels; i++) {
        ctx->cached_counts[i] = -1;
    }

    /* Initialize statistics */
    ctx->last_pivot_iteration = 0;
    ctx->total_candidates_scanned = 0;
    ctx->level_escalations = 0;

    return ctx;
}

/*============================================================================
 * PricingContext Destruction
 *===========================================================================*/

/**
 * @brief Free a pricing context and all its arrays.
 *
 * NULL-safe: does nothing if ctx is NULL.
 *
 * @param ctx Context to free
 */
void cxf_pricing_free(PricingContext *ctx) {
    if (ctx == NULL) {
        return;
    }

    /* Free candidate arrays per level */
    if (ctx->candidate_arrays != NULL) {
        for (int i = 0; i < ctx->max_levels; i++) {
            free(ctx->candidate_arrays[i]);
        }
        free(ctx->candidate_arrays);
    }

    free(ctx->candidate_counts);
    free(ctx->cached_counts);
    free(ctx);
}

/*============================================================================
 * PricingContext Initialization
 *===========================================================================*/

/**
 * @brief Initialize or reinitialize a pricing context.
 *
 * Resets the context to initial state for a new solve. Sets the
 * current level to 1 (partial pricing), invalidates all caches,
 * and resets statistics.
 *
 * @param ctx Context to initialize
 * @param num_vars Number of variables
 * @param strategy Pricing strategy (0=auto, 1=partial, 2=steepest edge)
 * @return CXF_OK on success, error code on failure
 */
int cxf_pricing_init(PricingContext *ctx, int num_vars, int strategy) {
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    (void)num_vars;   /* Will be used to build candidate lists in full impl */
    (void)strategy;   /* Will be used for strategy-specific setup */

    /* Reset to initial state */
    ctx->current_level = 1;

    /* Reset statistics */
    ctx->total_candidates_scanned = 0;
    ctx->level_escalations = 0;
    ctx->last_pivot_iteration = 0;

    /* Mark all caches as invalid */
    for (int i = 0; i < ctx->max_levels; i++) {
        ctx->cached_counts[i] = -1;
    }

    return CXF_OK;
}
