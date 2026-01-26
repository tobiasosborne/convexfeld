/**
 * @file init.c
 * @brief Full cxf_pricing_init implementation (M6.1.3)
 *
 * Initializes pricing context with strategy-specific data structures:
 * - Candidate arrays per level (sized based on strategy and num_vars)
 * - Steepest edge weights (if using SE or Devex)
 * - Statistics and cache state
 *
 * Spec: docs/specs/functions/pricing/cxf_pricing_init.md
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Pricing strategy constants */
#define STRATEGY_AUTO          0
#define STRATEGY_PARTIAL       1
#define STRATEGY_STEEPEST_EDGE 2
#define STRATEGY_DEVEX         3

/* Thresholds for auto-selection */
#define SMALL_PROBLEM_THRESHOLD 1000

/* Minimum candidates per level */
#define MIN_CANDIDATES 100

/*============================================================================
 * Helper: Compute candidate list size for a level
 *===========================================================================*/

/**
 * @brief Compute candidate array size for a pricing level.
 *
 * Level 0 (full): all variables
 * Level 1+: progressively smaller subsets, minimum MIN_CANDIDATES
 *
 * @param num_vars Total number of variables
 * @param level Pricing level (0=full)
 * @param strategy Pricing strategy
 * @return Number of candidates to allocate for this level
 */
static int compute_level_size(int num_vars, int level, int strategy) {
    if (level == 0) {
        /* Level 0 is full pricing - all variables */
        return num_vars;
    }

    if (strategy == STRATEGY_PARTIAL) {
        /* Partial pricing: sqrt(n) per level, minimum MIN_CANDIDATES */
        int size = (int)sqrt((double)num_vars);
        if (size < MIN_CANDIDATES) {
            size = MIN_CANDIDATES;
        }
        if (size > num_vars) {
            size = num_vars;
        }
        return size;
    }

    /* For SE/Devex, still allocate reasonable candidate lists */
    int size = num_vars / (1 << level);  /* n/2, n/4, n/8, ... */
    if (size < MIN_CANDIDATES) {
        size = MIN_CANDIDATES;
    }
    if (size > num_vars) {
        size = num_vars;
    }
    return size;
}

/*============================================================================
 * cxf_pricing_init - Full Implementation
 *===========================================================================*/

/**
 * @brief Initialize or reinitialize a pricing context for a new solve.
 *
 * Allocates candidate arrays based on strategy and problem size.
 * For steepest edge or Devex, allocates and initializes weight array.
 *
 * @param ctx Pricing context (must be created with cxf_pricing_create)
 * @param num_vars Number of variables in the problem
 * @param strategy Pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex)
 * @return CXF_OK on success, error code on failure
 */
int cxf_pricing_init(PricingContext *ctx, int num_vars, int strategy) {
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (num_vars < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Auto-select strategy if requested */
    int effective_strategy = strategy;
    if (strategy == STRATEGY_AUTO) {
        if (num_vars < SMALL_PROBLEM_THRESHOLD) {
            /* Small problems: use full pricing (Dantzig) */
            effective_strategy = STRATEGY_PARTIAL;  /* Actually uses full scan */
        } else {
            /* Large problems: use partial pricing */
            effective_strategy = STRATEGY_PARTIAL;
        }
    }

    /* Store configuration */
    ctx->num_vars = num_vars;
    ctx->strategy = effective_strategy;

    /* Reset to initial state */
    ctx->current_level = 1;

    /* Reset statistics */
    ctx->total_candidates_scanned = 0;
    ctx->level_escalations = 0;
    ctx->last_pivot_iteration = 0;

    /* Mark all caches as invalid */
    for (int i = 0; i < ctx->max_levels; i++) {
        ctx->cached_counts[i] = -1;
        ctx->candidate_counts[i] = 0;
    }

    /* Free any existing candidate arrays (for reinit case) */
    for (int i = 0; i < ctx->max_levels; i++) {
        free(ctx->candidate_arrays[i]);
        ctx->candidate_arrays[i] = NULL;
        ctx->candidate_sizes[i] = 0;
    }

    /* Allocate candidate arrays per level */
    if (num_vars > 0) {
        for (int level = 0; level < ctx->max_levels; level++) {
            int size = compute_level_size(num_vars, level, effective_strategy);
            ctx->candidate_arrays[level] = (int *)calloc((size_t)size, sizeof(int));
            if (ctx->candidate_arrays[level] == NULL) {
                /* Allocation failed - clean up and return error */
                for (int j = 0; j < level; j++) {
                    free(ctx->candidate_arrays[j]);
                    ctx->candidate_arrays[j] = NULL;
                    ctx->candidate_sizes[j] = 0;
                }
                return CXF_ERROR_OUT_OF_MEMORY;
            }
            ctx->candidate_sizes[level] = size;
        }
    }

    /* Handle steepest edge / Devex weights */
    free(ctx->weights);
    ctx->weights = NULL;

    if (effective_strategy == STRATEGY_STEEPEST_EDGE ||
        effective_strategy == STRATEGY_DEVEX) {
        if (num_vars > 0) {
            ctx->weights = (double *)malloc((size_t)num_vars * sizeof(double));
            if (ctx->weights == NULL) {
                /* Free candidate arrays on failure */
                for (int i = 0; i < ctx->max_levels; i++) {
                    free(ctx->candidate_arrays[i]);
                    ctx->candidate_arrays[i] = NULL;
                    ctx->candidate_sizes[i] = 0;
                }
                return CXF_ERROR_OUT_OF_MEMORY;
            }

            /* Initialize weights to 1.0 (unit reference frame) */
            for (int j = 0; j < num_vars; j++) {
                ctx->weights[j] = 1.0;
            }
        }
    }

    return CXF_OK;
}
