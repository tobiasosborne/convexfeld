/**
 * @file update.c
 * @brief Pricing update and invalidation functions (M6.1.6 + P4.4)
 *
 * V1: cxf_pricing_update, cxf_pricing_invalidate (retained for compat)
 * V2: cxf_pricing_update_queues — processes both V2 queues at current
 *     level: filter invalid entries, promote pending→committed, invalidate
 *     caches. Spec: pricing_core.md — cxf_pricing_update (P4.4)
 * Beads: pt31
 */

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"

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
 * deferred until SolverState integration.
 *
 * @param ctx Pricing context
 * @param entering_var Index of entering variable
 * @param leaving_row Leaving row index
 * @param pivot_column FTRAN result for entering column [num_rows]
 * @param pivot_row BTRAN result for leaving row (unused currently)
 * @param num_rows Number of rows in basis
 * @return CXF_OK on success, error code on failure
 */
int cxf_pricing_update(PricingState *ctx, int entering_var, int leaving_row,
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

/*===========================================================================
 * V2 Queue Consumer (P4.4) — cxf_pricing_update_queues
 *
 * Processes both V2 queues at current level per pricing_core.md Phase 3.
 * Phase activation guard → status filter → flag promote/demote → cache
 * invalidation.
 *===========================================================================*/

/* Flag bit constants (same as queue_insert.c) */
#define L1_COMMITTED 0x01
#define L1_PENDING   0x02
#define L1_MASK      0x03
#define L2_COMMITTED 0x04
#define L2_PENDING   0x08
#define L2_MASK      0x0C

/**
 * @brief Filter one queue at level 0: discard basic variables, compact.
 *
 * For variable queues: keep nonbasic (status < 0), discard basic (>= 0).
 * For constraint queues: keep all valid indices.
 * The is_var_queue flag selects the filtering mode.
 */
static int filter_queue_l0(int *queue, int total, const int *status,
                           int status_count, int is_var_queue) {
    int write = 0;
    for (int i = 0; i < total; i++) {
        int idx = queue[i];
        if (idx < 0 || idx >= status_count) continue;
        if (is_var_queue) {
            /* Variable queue: keep nonbasic variables (status < 0) */
            if (status[idx] < 0)
                queue[write++] = idx;
        } else {
            /* Constraint queue: keep all valid constraint indices */
            queue[write++] = idx;
        }
    }
    return write;
}

/**
 * @brief Filter one queue at levels 1-2: promote pending, demote stale.
 *
 * Variable queues discard basic (status >= 0) entries.
 * Constraint queues keep all valid entries.
 */
static int filter_queue_lx(int *queue, int total, uint8_t *flags,
                           int flag_count, const int *status,
                           int status_count, uint8_t commit_bit,
                           uint8_t pend_bit, uint8_t level_mask,
                           int is_var_queue) {
    int write = 0;
    for (int i = 0; i < total; i++) {
        int idx = queue[i];
        if (idx < 0 || idx >= flag_count) continue;

        /* Status check: discard basic variables from var queues */
        if (is_var_queue && idx < status_count &&
            status != NULL && status[idx] >= 0) {
            flags[idx] &= (uint8_t)~level_mask;
            continue;
        }

        if (flags[idx] & pend_bit) {
            /* Pending → promote: set committed, clear pending */
            flags[idx] = (uint8_t)((flags[idx] | commit_bit) & ~pend_bit);
            queue[write++] = idx;
        } else {
            /* Stale → demote: clear all bits for this level */
            flags[idx] &= (uint8_t)~level_mask;
        }
    }
    return write;
}

void cxf_pricing_update_queues(PricingState *ctx, SolverState *state) {
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

    if (level == 0) {
        /* Level 0: simple status-based compaction */
        if (ctx->constr_queue[0] != NULL) {
            int n = filter_queue_l0(ctx->constr_queue[0],
                                    ctx->constr_q_total[0],
                                    var_status, total_vars, 0);
            ctx->constr_q_committed[0] = n;
            ctx->constr_q_total[0] = n;
        }
        if (ctx->var_queue[0] != NULL) {
            int n = filter_queue_l0(ctx->var_queue[0],
                                    ctx->var_q_total[0],
                                    var_status, total_vars, 1);
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
                cb, pb, lm, 0);
            ctx->constr_q_committed[level] = n;
            ctx->constr_q_total[level] = n;
        }

        /* Process variable queue (is_var_queue=1) */
        if (ctx->var_queue[level] != NULL && ctx->var_flags != NULL) {
            int n = filter_queue_lx(ctx->var_queue[level],
                ctx->var_q_total[level], ctx->var_flags,
                ctx->num_vars, var_status, total_vars,
                cb, pb, lm, 1);
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

    /* Work counter */
    if (state->work_counter != NULL) {
        *state->work_counter += (double)(ctx->constr_q_total[level] +
                                          ctx->var_q_total[level]);
    }
}
