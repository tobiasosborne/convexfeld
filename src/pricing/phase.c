/**
 * @file phase.c
 * @brief Phase-2 specific pricing functions (M6.1.7)
 *
 * Implements cxf_pricing_step2 for fallback/completeness pricing
 * when partial pricing fails to find a candidate.
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Variable status codes */
#define VAR_BASIC        0   /* Basic variable */
#define VAR_AT_LOWER    -1   /* Nonbasic at lower bound */
#define VAR_AT_UPPER    -2   /* Nonbasic at upper bound */
#define VAR_FREE        -3   /* Free (superbasic) */

/**
 * @brief Full scan for any attractive variable (phase 2 / fallback).
 *
 * Used when partial pricing at higher levels fails to find an
 * improving variable. Performs a complete scan of all nonbasic variables.
 *
 * For partial pricing: ensures no candidate is missed by scanning all sections.
 * For SE/Devex: confirms optimality after first pass.
 * For Dantzig: already complete, so confirms optimality.
 *
 * @param ctx Pricing context
 * @param reduced_costs Reduced costs array [num_vars]
 * @param var_status Variable status array [num_vars]
 * @param num_vars Number of variables
 * @param tolerance Optimality tolerance
 * @return Index of entering variable, or -1 if optimal
 */
int cxf_pricing_step2(PricingContext *ctx, const double *reduced_costs,
                      const int *var_status, int num_vars, double tolerance) {
    if (ctx == NULL || reduced_costs == NULL || var_status == NULL) {
        return -1;
    }

    /* Full scan of all nonbasic variables */
    double best_violation = 0.0;
    int best_var = -1;

    for (int j = 0; j < num_vars; j++) {
        /* Skip basic variables */
        if (var_status[j] >= 0) {
            continue;
        }

        double rc = reduced_costs[j];

        /* Check attractiveness based on bound status */
        if (var_status[j] == VAR_AT_LOWER && rc < -tolerance) {
            /* At lower bound with negative RC: can increase */
            double violation = -rc;
            if (violation > best_violation) {
                best_violation = violation;
                best_var = j;
            }
        } else if (var_status[j] == VAR_AT_UPPER && rc > tolerance) {
            /* At upper bound with positive RC: can decrease */
            if (rc > best_violation) {
                best_violation = rc;
                best_var = j;
            }
        } else if (var_status[j] == VAR_FREE) {
            /* Free variable: attractive if |RC| > tolerance */
            double abs_rc = (rc < 0) ? -rc : rc;
            if (abs_rc > tolerance && abs_rc > best_violation) {
                best_violation = abs_rc;
                best_var = j;
            }
        }
    }

    /* Update statistics */
    if (ctx->total_candidates_scanned >= 0) {
        ctx->total_candidates_scanned += num_vars;
    }

    return best_var;
}
