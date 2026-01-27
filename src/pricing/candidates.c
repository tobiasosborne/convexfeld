/**
 * @file candidates.c
 * @brief Full cxf_pricing_candidates implementation (M6.1.4)
 *
 * Select candidate entering variables based on reduced cost violations.
 * Supports partial pricing (section cycling) and sorting by attractiveness.
 *
 * Spec: docs/specs/functions/pricing/cxf_pricing_candidates.md
 */

#define _GNU_SOURCE  /* for qsort_r on Linux */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Variable status codes */
#define VAR_AT_LOWER   -1
#define VAR_AT_UPPER   -2
#define VAR_FREE       -3

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
int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
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
