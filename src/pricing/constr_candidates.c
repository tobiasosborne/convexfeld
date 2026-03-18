/**
 * @file constr_candidates.c
 * @brief V2 adaptive constraint candidate retrieval (P3.18)
 *
 * V2: cxf_pricing_constr_candidates_v2 -- multi-level constraint queue
 *     retrieval with adaptive strategy (full scan vs partial expansion).
 * Also: cxf_pricing_get_constr_stats -- committed constraint count accessor.
 *
 * Mirrors candidates.c (variable-side) for the constraint side.
 *
 * Spec: pricing_support.md -- cxf_pricing_get_constr_stats (P3.18)
 *       pricing_core.md -- constraint candidate retrieval
 * Beads: convexfeld-ro2u
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "pricing_internal.h"

/*===========================================================================
 * Variable Stats Accessor (P3.18) — symmetric mirror of constr_stats
 *===========================================================================*/

void cxf_pricing_get_var_stats(PricingState *ctx, int *count_out,
                               int **queue_out) {
    if (!ctx || !count_out || !queue_out) {
        if (count_out) *count_out = 0;
        if (queue_out) *queue_out = NULL;
        return;
    }
    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) {
        *count_out = 0;
        *queue_out = NULL;
        return;
    }
    *count_out = ctx->var_q_committed[level];
    *queue_out = ctx->var_queue[level];
}

/*===========================================================================
 * Constraint Stats Accessor (P3.18)
 *===========================================================================*/

void cxf_pricing_get_constr_stats(PricingState *ctx, int *count_out,
                                  int **queue_out) {
    if (!ctx || !count_out || !queue_out) {
        if (count_out) *count_out = 0;
        if (queue_out) *queue_out = NULL;
        return;
    }
    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) {
        *count_out = 0;
        *queue_out = NULL;
        return;
    }
    *count_out = ctx->constr_q_committed[level];
    *queue_out = ctx->constr_queue[level];
}

/*===========================================================================
 * V2 Adaptive Constraint Candidate Retrieval (P3.18)
 *
 * Mirrors cxf_pricing_candidates (candidates.c) with var<->constr swap.
 * Level 0: base dirty list (O(1)). Levels 1-2: cached or adaptive.
 * Three threshold checks: expansion multiplier, coverage, work factor.
 *===========================================================================*/

/* Adaptive strategy thresholds (pricing_core.md) */
#define EXPANSION_MULTIPLIER  2.0
#define COVERAGE_FRACTION     0.5
#define EXPANSION_WORK_FACTOR 5e-4

void cxf_pricing_constr_candidates_v2(PricingState *ctx, SolverState *state,
                                      int *count_out, int **candidates_out) {
    if (ctx == NULL || state == NULL || count_out == NULL ||
        candidates_out == NULL) {
        if (count_out) *count_out = 0;
        if (candidates_out) *candidates_out = NULL;
        return;
    }

    int level = ctx->current_level;
    if (level < 0 || level >= CXF_MAX_PRICING_LEVELS) {
        *count_out = 0; *candidates_out = NULL; return;
    }

    /* Level 0 fast path: return committed constraint queue directly */
    if (level == 0) {
        *count_out = ctx->constr_q_committed[0];
        *candidates_out = ctx->constr_queue[0];
        return;
    }

    /* Cache check at levels 1-2 */
    if (ctx->cached_constr_count[level] >= 0) {
        *count_out = ctx->cached_constr_count[level];
        *candidates_out = ctx->constr_output_buf[level];
        return;
    }

    /* Cache miss -- compute candidate list with adaptive strategy */
    int *out_buf = ctx->constr_output_buf[level];
    if (out_buf == NULL) {
        *count_out = 0; *candidates_out = NULL; return;
    }

    int m = ctx->num_constrs;
    int result_count = 0;

    /* Three threshold checks to decide full scan vs partial expansion */
    int use_full_scan = 0;

    /* Check 1: Cross-queue density (var committed -> constr threshold) */
    int cross_committed = ctx->var_q_committed[level];
    if (m <= (int)(cross_committed * EXPANSION_MULTIPLIER))
        use_full_scan = 1;

    /* Check 2: Coverage fraction */
    if (!use_full_scan) {
        int constr_total = ctx->constr_q_total[level];
        if (m <= (int)(constr_total * COVERAGE_FRACTION))
            use_full_scan = 1;
    }

    /* Check 3: Expansion cost estimate (via CSC columns of var queue) */
    if (!use_full_scan && state->csc_col_ptr != NULL) {
        int64_t est_neighbors = 0;
        int *vq = ctx->var_queue[level];
        int vq_n = ctx->var_q_committed[level];
        for (int i = 0; i < vq_n && i < ctx->num_vars; i++) {
            int vi = vq[i];
            if (state->csc_col_ptr != NULL && vi >= 0 &&
                vi < state->num_vars) {
                est_neighbors += state->csc_col_ptr[vi + 1] -
                                 state->csc_col_ptr[vi];
            }
        }
        int constr_total = ctx->constr_q_total[level];
        if (est_neighbors +
            (int64_t)(constr_total * EXPANSION_WORK_FACTOR) >= (int64_t)m)
            use_full_scan = 1;
    }

    /* Constraint status: var_status[num_vars + i] >= 0 means valid */
    int *var_status = (state->basis) ? state->basis->var_status : NULL;
    int n_vars = state->num_vars;

    if (use_full_scan) {
        /* Full scan: iterate all constraints, keep valid status */
        for (int i = 0; i < m && result_count < m; i++) {
            if (var_status != NULL && var_status[n_vars + i] >= 0)
                out_buf[result_count++] = i;
        }
        if (state->work_counter)
            *state->work_counter += (double)m;
    } else {
        /* Partial expansion: seed + expand via cross-queue */
        uint8_t sel_bit = 0x10;

        /* Step 1: Seed from constraint queue */
        int *cq = ctx->constr_queue[level];
        int cq_n = ctx->constr_q_committed[level];
        for (int i = 0; i < cq_n; i++) {
            int ci = cq[i];
            if (ci >= 0 && ci < m && ctx->constr_flags != NULL) {
                out_buf[result_count++] = ci;
                ctx->constr_flags[ci] |= sel_bit;
            }
        }

        /* Step 2: Expand via cross-queue (var queue -> CSC cols -> rows) */
        int *vq = ctx->var_queue[level];
        int vq_n = ctx->var_q_committed[level];
        for (int i = 0; i < vq_n; i++) {
            int vi = vq[i];
            if (vi < 0 || vi >= state->num_vars) continue;
            if (state->csc_col_ptr == NULL) continue;

            int64_t cs = state->csc_col_ptr[vi];
            int64_t ce = state->csc_col_ptr[vi + 1];
            for (int64_t k = cs; k < ce; k++) {
                int row = state->csc_row_idx[k];
                if (row < 0 || row >= m) continue;
                if (ctx->constr_flags[row] & sel_bit) continue;
                out_buf[result_count++] = row;
                ctx->constr_flags[row] |= sel_bit;
                if (result_count >= m) break;
            }
            if (result_count >= m) break;
        }

        /* Step 3: Filter -- clear selection flags, keep valid status */
        int write = 0;
        for (int i = 0; i < result_count; i++) {
            int ci = out_buf[i];
            if (ci >= 0 && ci < m && ctx->constr_flags != NULL)
                ctx->constr_flags[ci] &= (uint8_t)~sel_bit;
            if (ci >= 0 && ci < m &&
                var_status != NULL && var_status[n_vars + ci] >= 0)
                out_buf[write++] = ci;
        }
        result_count = write;

        if (state->work_counter)
            *state->work_counter += (double)(cq_n + vq_n + result_count);
    }

    /* Cache result */
    ctx->cached_constr_count[level] = result_count;
    *count_out = result_count;
    *candidates_out = out_buf;
}
