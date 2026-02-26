/**
 * @file candidates.c
 * @brief Candidate retrieval: V1 (M6.1.4) + V2 adaptive (P4.5)
 *
 * V1: cxf_pricing_candidates — RC-based scan with section cycling
 * V2: cxf_pricing_candidates_v2 — multi-level queue retrieval with
 *     adaptive strategy (full scan vs partial expansion)
 *
 * Spec: pricing_core.md — cxf_pricing_candidates (P4.5)
 * Beads: v35i
 */

#define _GNU_SOURCE  /* for qsort_r on Linux */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "pricing_internal.h"

/* Variable status codes */

/* Pricing strategy constants */
#define STRATEGY_PARTIAL       1

/* Default number of sections for partial pricing */
#define DEFAULT_NUM_SECTIONS   10

/*============================================================================
 * Helper: Comparison for sorting by |reduced_cost| descending
 *===========================================================================*/

/**
 * @brief Compare two candidate indices by |reduced_cost| descending.
 * @param a First candidate index pointer
 * @param b Second candidate index pointer
 * @param context Pointer to reduced_costs array
 */
static int compare_by_abs_rc_desc(const void *a, const void *b, void *context) {
    const double *reduced_costs = (const double *)context;
    int idx_a = *(const int *)a;
    int idx_b = *(const int *)b;

    double abs_rc_a = fabs(reduced_costs[idx_a]);
    double abs_rc_b = fabs(reduced_costs[idx_b]);

    /* Sort descending: larger |RC| first */
    if (abs_rc_a > abs_rc_b) {
        return -1;
    } else if (abs_rc_a < abs_rc_b) {
        return 1;
    }
    return 0;
}

/*============================================================================
 * cxf_pricing_candidates - Full Implementation
 *===========================================================================*/

/**
 * @brief Find candidate entering variables.
 *
 * Scans nonbasic variables for attractive reduced costs:
 * - At lower bound: attractive if RC < -tolerance
 * - At upper bound: attractive if RC > tolerance
 * - Free variable: attractive if |RC| > tolerance
 *
 * For partial pricing, scans only a section of variables and advances
 * the section counter for next call. Candidates are sorted by |RC|
 * descending (most attractive first).
 *
 * @param ctx Pricing context
 * @param reduced_costs Reduced costs array [num_vars]
 * @param var_status Variable status array (>=0 basic, -1 at lower, -2 at upper, -3 free)
 * @param num_vars Number of variables
 * @param tolerance Optimality tolerance
 * @param candidates Output array for candidate indices
 * @param max_candidates Maximum candidates to return
 * @return Number of candidates found
 */
int cxf_pricing_candidates(PricingState *ctx, const double *reduced_costs,
                           const int *var_status, int num_vars, double tolerance,
                           int *candidates, int max_candidates) {
    if (ctx == NULL || reduced_costs == NULL || var_status == NULL ||
        candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    if (num_vars <= 0) {
        return 0;
    }

    /* Determine scan range based on pricing strategy */
    int start_idx = 0;
    int end_idx = num_vars;

    /* Partial pricing: scan only current section */
    if (ctx->strategy == STRATEGY_PARTIAL && num_vars > DEFAULT_NUM_SECTIONS) {
        int section_size = num_vars / DEFAULT_NUM_SECTIONS;
        int current_section = ctx->last_pivot_iteration % DEFAULT_NUM_SECTIONS;
        start_idx = current_section * section_size;
        end_idx = start_idx + section_size;
        if (end_idx > num_vars) {
            end_idx = num_vars;
        }
        /* Last section may be larger to include remainder */
        if (current_section == DEFAULT_NUM_SECTIONS - 1) {
            end_idx = num_vars;
        }
    }

    /* Scan for attractive nonbasic variables */
    int count = 0;
    int64_t scanned = 0;

    for (int j = start_idx; j < end_idx; j++) {
        scanned++;

        /* Skip basic variables (status >= 0 means row index in basis) */
        if (var_status[j] >= 0) {
            continue;
        }

        double rc = reduced_costs[j];
        int attractive = 0;

        if (var_status[j] == VAR_AT_LOWER && rc < -tolerance) {
            attractive = 1;  /* At lower, negative RC -> can improve */
        } else if (var_status[j] == VAR_AT_UPPER && rc > tolerance) {
            attractive = 1;  /* At upper, positive RC -> can improve */
        } else if (var_status[j] == VAR_FREE && fabs(rc) > tolerance) {
            attractive = 1;  /* Free variable with nonzero RC */
        }

        if (attractive) {
            if (count < max_candidates) {
                candidates[count++] = j;
            } else {
                /* Array full - find and replace least attractive if better */
                double new_abs_rc = fabs(rc);
                int min_idx = 0;
                double min_abs_rc = fabs(reduced_costs[candidates[0]]);

                for (int k = 1; k < count; k++) {
                    double abs_rc_k = fabs(reduced_costs[candidates[k]]);
                    if (abs_rc_k < min_abs_rc) {
                        min_abs_rc = abs_rc_k;
                        min_idx = k;
                    }
                }

                if (new_abs_rc > min_abs_rc) {
                    candidates[min_idx] = j;
                }
            }
        }
    }

    /* Update statistics */
    ctx->total_candidates_scanned += scanned;

    /* Sort candidates by |reduced_cost| descending */
    if (count > 1) {
        qsort_r(candidates, (size_t)count, sizeof(int),
                compare_by_abs_rc_desc, (void *)reduced_costs);
    }

    return count;
}

/*===========================================================================
 * V2 Adaptive Candidate Retrieval (P4.5)
 *
 * Returns pre-populated dirty variable queue at current level.
 * Level 0: base dirty list (O(1)). Levels 1-2: cached or adaptive.
 * Three threshold checks: expansion multiplier, coverage, work factor.
 *===========================================================================*/

/* Adaptive strategy thresholds (pricing_core.md) */
#define EXPANSION_MULTIPLIER  2.0
#define COVERAGE_FRACTION     0.5
#define EXPANSION_WORK_FACTOR 5e-4

void cxf_pricing_candidates_v2(PricingState *ctx, SolverState *state,
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

    /* Cache miss — compute candidate list with adaptive strategy */
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

        /* Step 2: Expand via cross-queue (constraint queue → CSR rows) */
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

        /* Step 3: Filter — clear selection flags, keep valid status */
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
