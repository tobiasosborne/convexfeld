/**
 * @file queue.c
 * @brief Pricing queue operations — V1 + V2 (P4.6/P4.7/P4.8)
 *
 * Dirty marking, cascade updates, level management, constraint candidates.
 * P4.7: mark_dirty/mark_constr_dirty now also insert into V2 4-bit queues.
 * P4.6: end_level now processes V2 queues with flag-based promote/demote.
 *
 * Spec: pricing_support.md, pricing_core.md
 * Beads: 52go (P4.7), fevq (P4.6)
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

#include "pricing_internal.h"

/* V2 insertion helpers (queue_insert.c) */

/* V2 queue consumer (update.c) */

/**
 * @brief Mark a variable as dirty (P4.7: V1 + V2 flag-based insertion).
 */
void cxf_pricing_mark_dirty(PricingState *ctx, int var_idx) {
    if (ctx == NULL || var_idx < 0) return;

    /* V1: boolean dirty flag (used by step2/step3/perturbation) */
    if (ctx->var_dirty != NULL && var_idx < ctx->num_vars &&
        !ctx->var_dirty[var_idx]) {
        ctx->var_dirty[var_idx] = 1;
        ctx->num_dirty++;
    }

    /* V2 (P4.7): 4-bit flag insertion into levels 1-2 */
    v2_insert_var(ctx, var_idx);
}

/**
 * @brief Mark a constraint as dirty (P4.7: V1 + V2 flag-based insertion).
 */
void cxf_pricing_mark_constr_dirty(PricingState *ctx, int constr_idx) {
    if (ctx == NULL || constr_idx < 0) return;

    /* V1: boolean dirty flag */
    if (ctx->constr_dirty != NULL && constr_idx < ctx->num_constrs &&
        !ctx->constr_dirty[constr_idx]) {
        ctx->constr_dirty[constr_idx] = 1;
        ctx->num_constr_dirty++;
    }

    /* V2 (P4.7): 4-bit flag insertion into levels 1-2 */
    v2_insert_constr(ctx, constr_idx);
}

/**
 * @brief Cascade dirty marks after a variable change (v2 P3.18).
 */
void cxf_pricing_cascade_update(PricingState *ctx, SolverState *state,
                                int var_idx) {
    if (ctx == NULL || state == NULL || var_idx < 0) return;
    cxf_pricing_mark_dirty(ctx, var_idx);

    if (state->csc_col_ptr == NULL || var_idx >= state->num_vars) return;

    int64_t start = state->csc_col_ptr[var_idx];
    int64_t end = state->csc_col_ptr[var_idx + 1];
    for (int64_t k = start; k < end; k++)
        cxf_pricing_mark_constr_dirty(ctx, state->csc_row_idx[k]);
}

/**
 * @brief Complete current pricing level (P4.6: V2 queue compaction).
 *
 * V1: Clears dirty flags, invalidates V1 caches.
 * V2: Delegates to cxf_pricing_update_queues for flag-based
 *     promote/demote and cache invalidation at levels 1-2.
 */
void cxf_pricing_end_level(PricingState *ctx) {
    if (ctx == NULL) return;

    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) return;

    /* V2 spec: Lazy activation guard (pricing_support.md).
     * First call at this level just marks it active and returns.
     * This ensures the first batch of dirty entries is treated correctly. */
    if (!ctx->level_active[level]) {
        ctx->level_active[level] = 1;
        return;
    }

    /* V1: Invalidate V1 caches */
    if (ctx->cached_counts != NULL) {
        for (int i = 0; i < ctx->max_levels; i++)
            ctx->cached_counts[i] = -1;
    }

    /* V1: Clear dirty flags */
    if (ctx->var_dirty != NULL && ctx->num_vars > 0) {
        memset(ctx->var_dirty, 0, (size_t)ctx->num_vars * sizeof(int));
        ctx->num_dirty = 0;
    }

    /* V2 (P4.6): Queue filtering is handled by cxf_pricing_update_queues
     * (update.c) which has access to SolverState for status-based filtering.
     * That function is called by step.c before pricing. Here we invalidate
     * V2 caches AFTER filtering, per pricing_support.md Phase 3. */
    ctx->cached_var_count[level] = -1;
    ctx->cached_var_count2[level] = -1;
    ctx->cached_var_count3[level] = -1;
    ctx->cached_constr_count[level] = -1;
    ctx->cached_constr_count2[level] = -1;
    ctx->cached_constr_count3[level] = -1;
}

/**
 * @brief Set active pricing level.
 */
void cxf_pricing_set_level(PricingState *ctx, int level) {
    if (ctx == NULL) return;
    if (level >= 0 && level < ctx->max_levels)
        ctx->current_level = level;
}

/**
 * @brief Get constraint candidates for phase_end/step3 processing.
 */
int cxf_pricing_get_constr_candidates(PricingState *ctx, int *out,
                                      int max_out) {
    if (ctx == NULL || out == NULL || max_out <= 0) return 0;
    if (ctx->constr_dirty == NULL) return 0;

    int count = 0;
    for (int i = 0; i < ctx->num_constrs && count < max_out; i++) {
        if (ctx->constr_dirty[i])
            out[count++] = i;
    }

    if (ctx->num_constrs > 0) {
        memset(ctx->constr_dirty, 0,
               (size_t)ctx->num_constrs * sizeof(int));
        ctx->num_constr_dirty = 0;
    }

    return count;
}
