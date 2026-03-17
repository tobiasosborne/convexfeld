/**
 * @file invalidate.c
 * @brief Pricing invalidation: single-var dirty marker + cache reset
 *
 * cxf_pricing_invalidate: V2 spec single-variable dirty marker
 *   (pricing_core.md §cxf_pricing_invalidate).
 * cxf_pricing_invalidate_cache: cache/weight reset (legacy, renamed).
 *
 * Split from update.c to keep files under 200 LOC.
 * Beads: pt31, convexfeld-u82g
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "pricing_internal.h"

/**
 * @brief Mark a single variable as dirty in both L1 and L2 queues.
 *
 * V2 spec (pricing_core.md §cxf_pricing_invalidate):
 * Direct dirty marking producer — adds varIndex to both level-1 and
 * level-2 variable queues using the flag-based O(1) duplicate protocol.
 * Also sets the V1 dirty flag for backward compatibility.
 *
 * @param ctx  Pricing state
 * @param varIndex  Index of the variable to mark dirty
 */
void cxf_pricing_invalidate(PricingState *ctx, int varIndex) {
    if (ctx == NULL || varIndex < 0) return;
    if (varIndex >= ctx->num_vars) return;

    /* V1: boolean dirty flag (used by step2/step3/perturbation) */
    if (ctx->var_dirty != NULL && !ctx->var_dirty[varIndex]) {
        ctx->var_dirty[varIndex] = 1;
        ctx->num_dirty++;
    }

    /* V2: 4-bit flag insertion into levels 1-2 */
    v2_insert_var(ctx, varIndex);
}

/**
 * @brief Invalidate cached pricing information (cache reset).
 *
 * Resets cached candidate counts and/or weights. Called at phase
 * transitions and after refactorization.
 *
 * @param ctx   Pricing context
 * @param flags Bitmask of CXF_INVALID_* flags
 */
void cxf_pricing_invalidate_cache(PricingState *ctx, int flags) {
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
        if (ctx->weights != NULL && ctx->num_vars > 0) {
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
