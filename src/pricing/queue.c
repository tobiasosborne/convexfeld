/**
 * @file queue.c
 * @brief V2 pricing queue operations (F1 — P3.17/P3.18)
 *
 * Dirty marking, cascade updates, level management, constraint candidates.
 * Spec: docs/specs-v2/specs/modules/pricing_core.md,
 *        docs/specs-v2/specs/modules/pricing_support.md
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Mark a variable as dirty (needing repricing).
 */
void cxf_pricing_mark_dirty(PricingState *ctx, int var_idx) {
    if (ctx == NULL || var_idx < 0) return;
    if (ctx->var_dirty == NULL || var_idx >= ctx->num_vars) return;
    if (!ctx->var_dirty[var_idx]) {
        ctx->var_dirty[var_idx] = 1;
        ctx->num_dirty++;
    }
}

/**
 * @brief Mark a constraint as dirty (needing propagation evaluation).
 */
void cxf_pricing_mark_constr_dirty(PricingState *ctx, int constr_idx) {
    if (ctx == NULL || constr_idx < 0) return;
    if (ctx->constr_dirty == NULL || constr_idx >= ctx->num_constrs) return;
    if (!ctx->constr_dirty[constr_idx]) {
        ctx->constr_dirty[constr_idx] = 1;
        ctx->num_constr_dirty++;
    }
}

/**
 * @brief Cascade dirty marks after a variable change (v2 P3.18).
 *
 * Marks the variable dirty and traverses its CSC column to mark
 * all structurally adjacent constraints as dirty. This feeds
 * step2/step3 bound propagation with their candidate queues.
 */
void cxf_pricing_cascade_update(PricingState *ctx, SolverState *state,
                                int var_idx) {
    if (ctx == NULL || state == NULL || var_idx < 0) return;
    cxf_pricing_mark_dirty(ctx, var_idx);

    /* Traverse owned CSC column to mark affected constraints (P3.1) */
    if (state->csc_col_ptr == NULL || var_idx >= state->num_vars) return;

    int64_t start = state->csc_col_ptr[var_idx];
    int64_t end = state->csc_col_ptr[var_idx + 1];
    for (int64_t k = start; k < end; k++) {
        cxf_pricing_mark_constr_dirty(ctx, state->csc_row_idx[k]);
    }
}

/**
 * @brief Complete current pricing level.
 *
 * Promotes/demotes candidates based on status filtering.
 * Invalidates caches.
 */
void cxf_pricing_end_level(PricingState *ctx) {
    if (ctx == NULL) return;

    /* Invalidate caches */
    if (ctx->cached_counts != NULL) {
        for (int i = 0; i < ctx->max_levels; i++) {
            ctx->cached_counts[i] = -1;
        }
    }

    /* Clear dirty flags */
    if (ctx->var_dirty != NULL && ctx->num_vars > 0) {
        memset(ctx->var_dirty, 0, (size_t)ctx->num_vars * sizeof(int));
        ctx->num_dirty = 0;
    }
}

/**
 * @brief Set active pricing level.
 */
void cxf_pricing_set_level(PricingState *ctx, int level) {
    if (ctx == NULL) return;
    if (level >= 0 && level < ctx->max_levels) {
        ctx->current_level = level;
    }
}

/**
 * @brief Get constraint candidates for phase_end/step3 processing.
 *
 * Returns dirty constraints that need propagation evaluation.
 */
int cxf_pricing_get_constr_candidates(PricingState *ctx, int *out,
                                      int max_out) {
    if (ctx == NULL || out == NULL || max_out <= 0) return 0;
    if (ctx->constr_dirty == NULL) return 0;

    int count = 0;
    for (int i = 0; i < ctx->num_constrs && count < max_out; i++) {
        if (ctx->constr_dirty[i]) {
            out[count++] = i;
        }
    }

    /* Clear constraint dirty flags after retrieval */
    if (ctx->num_constrs > 0) {
        memset(ctx->constr_dirty, 0,
               (size_t)ctx->num_constrs * sizeof(int));
        ctx->num_constr_dirty = 0;
    }

    return count;
}
