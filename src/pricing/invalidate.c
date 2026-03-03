/**
 * @file invalidate.c
 * @brief Pricing cache invalidation (M6.1.6)
 *
 * cxf_pricing_invalidate: reset cached candidates/weights on demand.
 * Spec: docs/specs/functions/pricing/cxf_pricing_invalidate.md
 *
 * Split from update.c to keep files under 200 LOC.
 * Beads: pt31
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "pricing_internal.h"

/**
 * @brief Invalidate cached pricing information.
 *
 * Sets flags indicating what pricing data needs recomputation.
 * The next pricing operation checks these flags and recomputes as needed.
 *
 * @param ctx Pricing context
 * @param flags Bitmask of CXF_INVALID_* flags
 */
void cxf_pricing_invalidate(PricingState *ctx, int flags) {
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
