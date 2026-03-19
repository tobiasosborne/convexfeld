/**
 * @file candidates.c
 * @brief V2 adaptive candidate retrieval with eta-mode expansion (P4.5).
 * Spec: pricing_core.md. V1 in candidates_v1.c. Beads: v35i
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "pricing_internal.h"

/* Adaptive strategy thresholds (pricing_core.md) */
#define EXPANSION_MULTIPLIER  2.0
#define COVERAGE_FRACTION     0.5
#define EXPANSION_WORK_FACTOR 5e-4

void cxf_pricing_candidates(PricingState *ctx, SolverState *state,
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

    /* Level 0 fast path: return committed variable queue directly */
    if (level == 0) {
        *count_out = ctx->var_q_committed[0];
        *candidates_out = ctx->var_queue[0];
        return;
    }

    /* Cache check at levels 1-2 */
    if (ctx->cached_var_count[level] >= 0) {
        *count_out = ctx->cached_var_count[level];
        *candidates_out = ctx->var_output_buf[level];
        return;
    }

    /* Cache miss -- compute candidate list with adaptive strategy */
    int *out_buf = ctx->var_output_buf[level];
    if (out_buf == NULL) {
        *count_out = 0; *candidates_out = NULL; return;
    }

    int n = ctx->num_vars;
    int *var_status = (state->basis) ? state->basis->var_status : NULL;
    int result_count = 0;

    /* Three threshold checks to decide full scan vs partial expansion */
    int use_full_scan = 0;

    /* Check 1: Cross-queue density */
    int cross_committed = ctx->constr_q_committed[level];
    if (n <= (int)(cross_committed * EXPANSION_MULTIPLIER))
        use_full_scan = 1;

    /* Check 2: Coverage fraction */
    if (!use_full_scan) {
        int var_total = ctx->var_q_total[level];
        if (n <= (int)(var_total * COVERAGE_FRACTION))
            use_full_scan = 1;
    }

    /* Check 3: Expansion cost estimate */
    if (!use_full_scan && state->csc_col_ptr != NULL) {
        int64_t est_neighbors = 0;
        int *cq = ctx->constr_queue[level];
        int cq_n = ctx->constr_q_committed[level];
        for (int i = 0; i < cq_n && i < ctx->num_constrs; i++) {
            int ci = cq[i];
            /* Estimate: count nonzeros in each constraint's row */
            if (state->csr_row_ptr != NULL && ci >= 0 &&
                ci < state->num_constrs) {
                est_neighbors += state->csr_row_ptr[ci + 1] -
                                 state->csr_row_ptr[ci];
            }
        }
        /* Eta-mode cost: each pivot eta adds potential neighbors */
        int eta_count = 0;
        if (state->basis != NULL) {
            EtaVector *e = state->basis->eta_head;
            while (e != NULL) {
                if (e->type == CXF_ETA_PIVOT) eta_count++;
                e = e->next;
            }
        }
        est_neighbors += (int64_t)eta_count * cq_n;

        int var_total = ctx->var_q_total[level];
        if (est_neighbors + (int64_t)(var_total * EXPANSION_WORK_FACTOR) >=
            (int64_t)n)
            use_full_scan = 1;
    }

    if (use_full_scan) {
        /* Full scan: iterate all variables, keep nonbasic */
        for (int j = 0; j < n && result_count < n; j++) {
            if (var_status != NULL && var_status[j] < 0)
                out_buf[result_count++] = j;
        }
        if (state->work_counter)
            *state->work_counter += (double)n;
    } else {
        /* Partial expansion: seed + expand via cross-queue */
        /* Use var_flags as temporary selection marker (bit 4, unused) */
        uint8_t sel_bit = 0x10;

        /* Step 1: Seed from variable queue */
        int *vq = ctx->var_queue[level];
        int vq_n = ctx->var_q_committed[level];
        for (int i = 0; i < vq_n; i++) {
            int vi = vq[i];
            if (vi >= 0 && vi < n && ctx->var_flags != NULL) {
                out_buf[result_count++] = vi;
                ctx->var_flags[vi] |= sel_bit;
            }
        }

        /* Step 2a: Expand via cross-queue (constraint queue -> CSR rows) */
        int *cq = ctx->constr_queue[level];
        int cq_n = ctx->constr_q_committed[level];
        for (int i = 0; i < cq_n; i++) {
            int ci = cq[i];
            if (ci < 0 || ci >= state->num_constrs) continue;
            if (state->csr_row_ptr == NULL) continue;

            int64_t rs = state->csr_row_ptr[ci];
            int64_t re = state->csr_row_ptr[ci + 1];
            for (int64_t k = rs; k < re; k++) {
                int col = state->csr_col_idx[k];
                if (col < 0 || col >= n) continue;
                if (ctx->var_flags[col] & sel_bit) continue;
                out_buf[result_count++] = col;
                ctx->var_flags[col] |= sel_bit;
                if (result_count >= n) break;
            }
            if (result_count >= n) break;
        }

        /* Step 2b: Eta-mode expansion -- add dynamic neighbors from etas */
        if (result_count < n && state->basis != NULL) {
            EtaVector *eta = state->basis->eta_head;
            while (eta != NULL && result_count < n) {
                if (eta->type == CXF_ETA_PIVOT) {
                    for (int i = 0; i < cq_n; i++) {
                        if (cq[i] == eta->pivot_row) {
                            int ev = eta->entering_var;
                            if (ev >= 0 && ev < n &&
                                !(ctx->var_flags[ev] & sel_bit)) {
                                out_buf[result_count++] = ev;
                                ctx->var_flags[ev] |= sel_bit;
                            }
                            int lv = eta->leaving_var;
                            if (lv >= 0 && lv < n &&
                                result_count < n &&
                                !(ctx->var_flags[lv] & sel_bit)) {
                                out_buf[result_count++] = lv;
                                ctx->var_flags[lv] |= sel_bit;
                            }
                            break;
                        }
                    }
                }
                eta = eta->next;
            }
        }

        /* Step 3: Filter -- clear selection flags, keep valid status */
        int write = 0;
        for (int i = 0; i < result_count; i++) {
            int vi = out_buf[i];
            if (vi >= 0 && vi < n && ctx->var_flags != NULL)
                ctx->var_flags[vi] &= (uint8_t)~sel_bit;
            if (var_status != NULL && var_status[vi] < 0)
                out_buf[write++] = vi;
        }
        result_count = write;

        if (state->work_counter)
            *state->work_counter += (double)(vq_n + cq_n + result_count);
    }

    /* Cache result */
    ctx->cached_var_count[level] = result_count;
    *count_out = result_count;
    *candidates_out = out_buf;
}
