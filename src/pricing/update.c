/**
 * @file update.c
 * @brief V2 queue processing (P4.4)
 *
 * cxf_pricing_update: process V2 queues at current level —
 *     filter invalid entries, promote pending->committed, invalidate
 *     caches. Spec: pricing_core.md (P4.4)
 *
 * Cache invalidation (cxf_pricing_invalidate) is in invalidate.c.
 * Weight updates after pivot are in weight_update.c (P4.9).
 * Beads: pt31
 */

#include <stdint.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "pricing_internal.h"

/*===========================================================================
 * V2 Queue Consumer (P4.4) — cxf_pricing_update
 *
 * Processes both V2 queues at current level per pricing_core.md Phase 3.
 * Phase activation guard -> status filter -> flag promote/demote -> cache
 * invalidation.
 *===========================================================================*/

/* Flag bit constants (same as queue_insert.c) */

/**
 * @brief Filter one queue at level 0: discard invalid entries, compact.
 *
 * For variable queues: keep nonbasic (status < 0), discard basic (>= 0).
 * For constraint queues: keep entries whose constraint status (i.e.
 *   status[status_offset + idx]) is non-negative per pricing_support.md L0.
 *
 * @param status_offset  For constraint queues: num_vars (maps constr idx
 *                       to its slack var in the combined status array).
 *                       For variable queues: 0 (unused).
 */
static int filter_queue_l0(int *queue, int total, const int *status,
                           int status_count, int is_var_queue,
                           int status_offset) {
    int write = 0;
    for (int i = 0; i < total; i++) {
        int idx = queue[i];
        if (idx < 0) continue;
        if (is_var_queue) {
            /* Variable queue: keep nonbasic variables (status < 0) */
            if (idx >= status_count) continue;
            if (status != NULL && status[idx] < 0)
                queue[write++] = idx;
        } else {
            /* Constraint queue: keep entries with non-negative constraint
             * status.  Constraint idx maps to status[status_offset + idx]
             * (the slack variable for that constraint). */
            int si = status_offset + idx;
            if (si >= status_count) continue;
            if (status != NULL && status[si] >= 0)
                queue[write++] = idx;
        }
    }
    return write;
}

/**
 * @brief Filter one queue at levels 1-2: promote pending, demote stale.
 *
 * Variable queues discard basic (status >= 0) entries.
 * Constraint queues discard entries with negative constraint status
 * (status[status_offset + idx] < 0) per pricing_support.md L1-L2.
 *
 * @param status_offset  For constraint queues: num_vars.
 *                       For variable queues: 0 (unused).
 */
static int filter_queue_lx(int *queue, int total, uint8_t *flags,
                           int flag_count, const int *status,
                           int status_count, uint8_t commit_bit,
                           uint8_t pend_bit, uint8_t level_mask,
                           int is_var_queue, int status_offset) {
    int write = 0;
    for (int i = 0; i < total; i++) {
        int idx = queue[i];
        if (idx < 0 || idx >= flag_count) continue;

        /* Status check: discard invalid entries */
        if (status != NULL) {
            if (is_var_queue) {
                /* Variable queue: discard basic (status >= 0) */
                if (idx < status_count && status[idx] >= 0) {
                    flags[idx] &= (uint8_t)~level_mask;
                    continue;
                }
            } else {
                /* Constraint queue: discard negative constraint status */
                int si = status_offset + idx;
                if (si < status_count && status[si] < 0) {
                    flags[idx] &= (uint8_t)~level_mask;
                    continue;
                }
            }
        }

        if (flags[idx] & pend_bit) {
            /* Pending -> promote: set committed, clear pending */
            flags[idx] = (uint8_t)((flags[idx] | commit_bit) & ~pend_bit);
            queue[write++] = idx;
        } else {
            /* Stale -> demote: clear all bits for this level */
            flags[idx] &= (uint8_t)~level_mask;
        }
    }
    return write;
}

void cxf_pricing_update(PricingState *ctx, SolverState *state) {
    if (ctx == NULL || state == NULL) return;

    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) return;

    /* Phase activation guard: first call just activates */
    if (!ctx->level_active[level]) {
        ctx->level_active[level] = 1;
        return;
    }

    /* Get status arrays from basis (non-negative = basic = valid) */
    int *var_status = (state->basis != NULL) ? state->basis->var_status : NULL;

    int total_vars = state->num_vars + state->num_constrs;

    int num_vars = state->num_vars;

    /* Capture pre-compaction queue sizes for work counter.
     * Per pricing_support.md: work counter reflects entries SCANNED,
     * not entries surviving compaction. */
    int pre_constr_total = ctx->constr_q_total[level];
    int pre_var_total = ctx->var_q_total[level];

    if (level == 0) {
        /* Level 0: simple status-based compaction */
        if (ctx->constr_queue[0] != NULL) {
            int n = filter_queue_l0(ctx->constr_queue[0],
                                    ctx->constr_q_total[0],
                                    var_status, total_vars, 0,
                                    num_vars);
            ctx->constr_q_committed[0] = n;
            ctx->constr_q_total[0] = n;
        }
        if (ctx->var_queue[0] != NULL) {
            int n = filter_queue_l0(ctx->var_queue[0],
                                    ctx->var_q_total[0],
                                    var_status, total_vars, 1,
                                    0);
            ctx->var_q_committed[0] = n;
            ctx->var_q_total[0] = n;
        }
    } else {
        /* Levels 1-2: flag-based promote/demote */
        uint8_t cb = (level == 1) ? L1_COMMITTED : L2_COMMITTED;
        uint8_t pb = (level == 1) ? L1_PENDING : L2_PENDING;
        uint8_t lm = (level == 1) ? L1_MASK : L2_MASK;

        /* Process constraint queue (is_var_queue=0) */
        if (ctx->constr_queue[level] != NULL && ctx->constr_flags != NULL) {
            int n = filter_queue_lx(ctx->constr_queue[level],
                ctx->constr_q_total[level], ctx->constr_flags,
                ctx->num_constrs, var_status, total_vars,
                cb, pb, lm, 0, num_vars);
            ctx->constr_q_committed[level] = n;
            ctx->constr_q_total[level] = n;
        }

        /* Process variable queue (is_var_queue=1) */
        if (ctx->var_queue[level] != NULL && ctx->var_flags != NULL) {
            int n = filter_queue_lx(ctx->var_queue[level],
                ctx->var_q_total[level], ctx->var_flags,
                ctx->num_vars, var_status, total_vars,
                cb, pb, lm, 1, 0);
            ctx->var_q_committed[level] = n;
            ctx->var_q_total[level] = n;
        }

        /* Invalidate all 6 cache slots at this level */
        ctx->cached_var_count[level] = -1;
        ctx->cached_var_count2[level] = -1;
        ctx->cached_var_count3[level] = -1;
        ctx->cached_constr_count[level] = -1;
        ctx->cached_constr_count2[level] = -1;
        ctx->cached_constr_count3[level] = -1;
    }

    /* Work counter: use pre-compaction sizes (entries scanned) per
     * pricing_support.md Work Counter Integration. */
    if (state->work_counter != NULL) {
        *state->work_counter += (double)(pre_constr_total + pre_var_total);
    }
}
