/**
 * @file end_level.c
 * @brief cxf_pricing_end_level — V2 queue filtering per pricing_support.md.
 *
 * Completes the current pricing level: lazy activation on first call,
 * status-based compaction at L0, flag-based promote/demote at L1-L2,
 * cache invalidation, work counter increment.
 *
 * Spec: pricing_support.md (P3.18 cxf_pricing_end_level)
 * Beads: au3b (signature), 8nlv (queue filtering)
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

#include "pricing_internal.h"

/**
 * @brief Complete current pricing level (V2 pricing_support.md).
 *
 * Filters both constraint and variable queues at the current level,
 * performing status-based compaction (L0) or flag-based promote/demote
 * (L1-L2). Invalidates candidate caches at L1-L2. Increments work counter.
 *
 * @param ctx   Pricing subsystem state
 * @param state Solver state (status arrays + work counter). May be NULL
 *              (skips queue filtering, only does V1 cleanup + caches).
 */
void cxf_pricing_end_level(PricingState *ctx, SolverState *state) {
    if (ctx == NULL) return;

    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) return;

    /* V2 spec: Lazy activation guard (pricing_support.md).
     * First call at this level just marks it active and returns. */
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

    /* V2 (P4.6): Queue filtering per pricing_support.md.
     * Requires SolverState for status arrays. */
    if (state != NULL) {
        const int *vs = (state->basis) ? state->basis->var_status : NULL;
        int total_st = state->num_vars + state->num_constrs;
        int nv = state->num_vars;

        /* Pre-compaction queue sizes for work counter */
        int pre_var = ctx->var_q_total[level];
        int pre_con = ctx->constr_q_total[level];

        if (level == 0) {
            /* Level 0: simple status-based compaction.
             * Retain nonbasic variables (status < 0) — basic vars
             * cannot enter the basis and are ineligible for pricing. */
            if (ctx->var_queue[0] != NULL && vs != NULL) {
                int w = 0;
                for (int i = 0; i < ctx->var_q_total[0]; i++) {
                    int idx = ctx->var_queue[0][i];
                    if (idx >= 0 && idx < total_st && vs[idx] < 0)
                        ctx->var_queue[0][w++] = idx;
                }
                ctx->var_q_committed[0] = w;
                ctx->var_q_total[0] = w;
            }
            if (ctx->constr_queue[0] != NULL && vs != NULL) {
                int w = 0;
                for (int i = 0; i < ctx->constr_q_total[0]; i++) {
                    int idx = ctx->constr_queue[0][i];
                    int si = nv + idx;
                    if (idx >= 0 && si < total_st && vs[si] >= 0)
                        ctx->constr_queue[0][w++] = idx;
                }
                ctx->constr_q_committed[0] = w;
                ctx->constr_q_total[0] = w;
            }
        } else {
            /* Levels 1-2: flag-based promote/demote */
            uint8_t cb = (level == 1) ? L1_COMMITTED : L2_COMMITTED;
            uint8_t pb = (level == 1) ? L1_PENDING   : L2_PENDING;
            uint8_t lm = (level == 1) ? L1_MASK      : L2_MASK;

            /* Variable queue */
            if (ctx->var_queue[level] != NULL && ctx->var_flags != NULL) {
                int w = 0;
                for (int i = 0; i < ctx->var_q_total[level]; i++) {
                    int idx = ctx->var_queue[level][i];
                    if (idx < 0 || idx >= ctx->num_vars) continue;
                    if (vs != NULL && idx < total_st && vs[idx] >= 0) {
                        ctx->var_flags[idx] &= (uint8_t)~lm;
                        continue;
                    }
                    if (ctx->var_flags[idx] & pb) {
                        ctx->var_flags[idx] = (uint8_t)(
                            (ctx->var_flags[idx] | cb) & ~pb);
                        ctx->var_queue[level][w++] = idx;
                    } else {
                        ctx->var_flags[idx] &= (uint8_t)~lm;
                    }
                }
                ctx->var_q_committed[level] = w;
                ctx->var_q_total[level] = w;
            }

            /* Constraint queue */
            if (ctx->constr_queue[level] != NULL &&
                ctx->constr_flags != NULL) {
                int w = 0;
                for (int i = 0; i < ctx->constr_q_total[level]; i++) {
                    int idx = ctx->constr_queue[level][i];
                    if (idx < 0 || idx >= ctx->num_constrs) continue;
                    int si = nv + idx;
                    if (vs != NULL && si < total_st && vs[si] < 0) {
                        ctx->constr_flags[idx] &= (uint8_t)~lm;
                        continue;
                    }
                    if (ctx->constr_flags[idx] & pb) {
                        ctx->constr_flags[idx] = (uint8_t)(
                            (ctx->constr_flags[idx] | cb) & ~pb);
                        ctx->constr_queue[level][w++] = idx;
                    } else {
                        ctx->constr_flags[idx] &= (uint8_t)~lm;
                    }
                }
                ctx->constr_q_committed[level] = w;
                ctx->constr_q_total[level] = w;
            }

            /* Invalidate all 6 cache slots at this level */
            ctx->cached_var_count[level] = -1;
            ctx->cached_var_count2[level] = -1;
            ctx->cached_var_count3[level] = -1;
            ctx->cached_constr_count[level] = -1;
            ctx->cached_constr_count2[level] = -1;
            ctx->cached_constr_count3[level] = -1;
        }

        /* Work counter: pre-compaction sizes (entries scanned) */
        if (state->work_counter != NULL)
            *state->work_counter += (double)(pre_var + pre_con);
    } else {
        /* No SolverState: cache invalidation only (levels > 0) */
        if (level > 0) {
            ctx->cached_var_count[level] = -1;
            ctx->cached_var_count2[level] = -1;
            ctx->cached_var_count3[level] = -1;
            ctx->cached_constr_count[level] = -1;
            ctx->cached_constr_count2[level] = -1;
            ctx->cached_constr_count3[level] = -1;
        }
    }

    /* Set level's activity flag (spec postcondition) */
    ctx->level_active[level] = 1;
}
