/**
 * @file queue_insert.c
 * @brief V2 flag-based queue insertion helpers (P4.2/P4.3 shared)
 *
 * Implements the committed/pending queue insertion protocol from
 * pricing_core.md Phase 2. Uses 4-bit per-element flags for O(1)
 * duplicate prevention across two pricing levels.
 *
 * Flag encoding (per element byte):
 *   Bit 0: Level 1 committed    Bit 1: Level 1 pending
 *   Bit 2: Level 2 committed    Bit 3: Level 2 pending
 *
 * Spec: pricing_core.md, pricing_state.md
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <stdint.h>

/* Flag bit positions for each level */
#define L1_COMMITTED  0x01  /* bit 0 */
#define L1_PENDING    0x02  /* bit 1 */
#define L1_MASK       0x03  /* bits 0-1 */
#define L2_COMMITTED  0x04  /* bit 2 */
#define L2_PENDING    0x08  /* bit 3 */
#define L2_MASK       0x0C  /* bits 2-3 */

/**
 * @brief Insert a constraint index into V2 constraint queues at levels 1-2.
 *
 * Per pricing_core.md queue insertion protocol:
 * - If not already in queue (flag bits clear): insert
 *   - Phase inactive: committed section (both counts++)
 *   - Phase active: pending section (total++ only)
 * - If level phase active: set pending bit
 */
void v2_insert_constr(PricingState *ctx, int constr_idx) {
    if (ctx == NULL || ctx->constr_flags == NULL) return;
    if (constr_idx < 0 || constr_idx >= ctx->num_constrs) return;

    uint8_t *flag = &ctx->constr_flags[constr_idx];

    /* Level 1 insertion */
    if ((*flag & L1_MASK) == 0) {
        /* Not in level-1 queue — insert */
        if (!ctx->level_active[1]) {
            /* Phase inactive: insert into committed section */
            int pos = ctx->constr_q_committed[1];
            if (pos < ctx->constr_queue_cap[1]) {
                ctx->constr_queue[1][pos] = constr_idx;
                ctx->constr_q_committed[1]++;
                ctx->constr_q_total[1]++;
                *flag |= L1_COMMITTED;
            }
        } else {
            /* Phase active: insert into pending section */
            int pos = ctx->constr_q_total[1];
            if (pos < ctx->constr_queue_cap[1]) {
                ctx->constr_queue[1][pos] = constr_idx;
                ctx->constr_q_total[1]++;
            }
        }
    }
    /* Set pending bit if level 1 phase is active */
    if (ctx->level_active[1])
        *flag |= L1_PENDING;

    /* Level 2 insertion */
    if ((*flag & L2_MASK) == 0) {
        if (!ctx->level_active[2]) {
            int pos = ctx->constr_q_committed[2];
            if (pos < ctx->constr_queue_cap[2]) {
                ctx->constr_queue[2][pos] = constr_idx;
                ctx->constr_q_committed[2]++;
                ctx->constr_q_total[2]++;
                *flag |= L2_COMMITTED;
            }
        } else {
            int pos = ctx->constr_q_total[2];
            if (pos < ctx->constr_queue_cap[2]) {
                ctx->constr_queue[2][pos] = constr_idx;
                ctx->constr_q_total[2]++;
            }
        }
    }
    if (ctx->level_active[2])
        *flag |= L2_PENDING;
}

/**
 * @brief Insert a variable index into V2 variable queues at levels 1-2.
 *
 * Symmetric to v2_insert_constr but operates on variable queues.
 */
void v2_insert_var(PricingState *ctx, int var_idx) {
    if (ctx == NULL || ctx->var_flags == NULL) return;
    if (var_idx < 0 || var_idx >= ctx->num_vars) return;

    uint8_t *flag = &ctx->var_flags[var_idx];

    /* Level 1 insertion */
    if ((*flag & L1_MASK) == 0) {
        if (!ctx->level_active[1]) {
            int pos = ctx->var_q_committed[1];
            if (pos < ctx->var_queue_cap[1]) {
                ctx->var_queue[1][pos] = var_idx;
                ctx->var_q_committed[1]++;
                ctx->var_q_total[1]++;
                *flag |= L1_COMMITTED;
            }
        } else {
            int pos = ctx->var_q_total[1];
            if (pos < ctx->var_queue_cap[1]) {
                ctx->var_queue[1][pos] = var_idx;
                ctx->var_q_total[1]++;
            }
        }
    }
    if (ctx->level_active[1])
        *flag |= L1_PENDING;

    /* Level 2 insertion */
    if ((*flag & L2_MASK) == 0) {
        if (!ctx->level_active[2]) {
            int pos = ctx->var_q_committed[2];
            if (pos < ctx->var_queue_cap[2]) {
                ctx->var_queue[2][pos] = var_idx;
                ctx->var_q_committed[2]++;
                ctx->var_q_total[2]++;
                *flag |= L2_COMMITTED;
            }
        } else {
            int pos = ctx->var_q_total[2];
            if (pos < ctx->var_queue_cap[2]) {
                ctx->var_queue[2][pos] = var_idx;
                ctx->var_q_total[2]++;
            }
        }
    }
    if (ctx->level_active[2])
        *flag |= L2_PENDING;
}
