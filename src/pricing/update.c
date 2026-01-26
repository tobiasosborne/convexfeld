/**
 * @file update.c
 * @brief Pricing update and invalidation functions (M6.1.6)
 *
 * Implements cxf_pricing_update and cxf_pricing_invalidate for
 * maintaining pricing data structures after simplex pivots.
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Invalidation flags */
#define CXF_INVALID_CANDIDATES     0x01
#define CXF_INVALID_REDUCED_COSTS  0x02
#define CXF_INVALID_WEIGHTS        0x04
#define CXF_INVALID_ALL            0xFF

/* Minimum weight to prevent division by zero */
#define MIN_WEIGHT 1e-10

/**
 * @brief Update pricing context after a pivot operation.
 *
 * Updates steepest edge weights if SE pricing is active.
 * Invalidates cached candidate lists.
 *
 * Full reduced cost update requires solver state access, which is
 * deferred until SolverContext integration.
 *
 * @param ctx Pricing context
 * @param entering_var Index of entering variable
 * @param leaving_row Leaving row index
 * @param pivot_column FTRAN result for entering column [num_rows]
 * @param pivot_row BTRAN result for leaving row (unused currently)
 * @param num_rows Number of rows in basis
 * @return CXF_OK on success, error code on failure
 */
int cxf_pricing_update(PricingContext *ctx, int entering_var, int leaving_row,
                       const double *pivot_column, const double *pivot_row,
                       int num_rows) {
    if (ctx == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Get pivot element */
    double pivot_elem = 1.0;
    if (pivot_column != NULL && leaving_row >= 0 && leaving_row < num_rows) {
        pivot_elem = pivot_column[leaving_row];
    }

    /* Update steepest edge weights if using SE strategy */
    if (ctx->strategy == 2 && ctx->weights != NULL && pivot_column != NULL) {
        /* SE weight for entering variable (now basic) */
        double gamma_entering = ctx->weights[entering_var];
        double pivot_sq = pivot_elem * pivot_elem;

        /* Avoid division by zero */
        if (pivot_sq < MIN_WEIGHT) {
            pivot_sq = MIN_WEIGHT;
        }

        /* tau = gamma_entering / pivot_sq would be used in full SE update */
        (void)gamma_entering;  /* Suppress warning until full SE update */

        /* Update weights for nonbasic variables */
        /* Note: Full update requires alpha_j for each variable j,
         * which needs matrix access. For now, we just ensure
         * the entering variable's weight is handled. */

        /* Weight for entering variable is no longer needed (now basic) */
        ctx->weights[entering_var] = 1.0;  /* Reset for when it leaves basis */
    }

    /* Invalidate all cached candidate counts */
    for (int i = 0; i < ctx->max_levels; i++) {
        ctx->cached_counts[i] = -1;
    }

    /* Update iteration counter */
    ctx->last_pivot_iteration++;

    /* Suppress unused parameter warnings */
    (void)pivot_row;

    return CXF_OK;
}

/**
 * @brief Invalidate cached pricing information.
 *
 * Sets flags indicating what pricing data needs recomputation.
 * The next pricing operation checks these flags and recomputes as needed.
 *
 * @param ctx Pricing context
 * @param flags Bitmask of CXF_INVALID_* flags
 */
void cxf_pricing_invalidate(PricingContext *ctx, int flags) {
    if (ctx == NULL) {
        return;
    }

    /* Invalidate candidate lists */
    if (flags & CXF_INVALID_CANDIDATES) {
        for (int i = 0; i < ctx->max_levels; i++) {
            ctx->cached_counts[i] = -1;
            ctx->candidate_counts[i] = 0;
        }
    }

    /* Invalidate weights - mark for full recomputation */
    if (flags & CXF_INVALID_WEIGHTS) {
        /* Full weight recomputation will happen on next SE pricing call.
         * For now, weights array remains allocated but values are stale. */
        if (ctx->weights != NULL && ctx->num_vars > 0) {
            /* Reset to 1.0 as safe default */
            for (int i = 0; i < ctx->num_vars; i++) {
                ctx->weights[i] = 1.0;
            }
        }
    }

    /* Handle CXF_INVALID_ALL - invalidate everything */
    if (flags == CXF_INVALID_ALL) {
        for (int i = 0; i < ctx->max_levels; i++) {
            ctx->cached_counts[i] = -1;
            ctx->candidate_counts[i] = 0;
        }
        if (ctx->weights != NULL && ctx->num_vars > 0) {
            for (int i = 0; i < ctx->num_vars; i++) {
                ctx->weights[i] = 1.0;
            }
        }
    }
}
