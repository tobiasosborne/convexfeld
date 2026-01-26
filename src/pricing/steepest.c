/**
 * @file steepest.c
 * @brief Steepest edge pricing implementation (M6.1.5)
 *
 * Implements cxf_pricing_steepest - selecting the entering variable using
 * the steepest edge criterion. The SE ratio is |d_j| / sqrt(gamma_j) where
 * d_j is the reduced cost and gamma_j is the SE weight.
 *
 * Spec: docs/specs/functions/pricing/cxf_pricing_steepest.md
 */

#include <stdlib.h>
#include <math.h>
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Variable status codes */
#define VAR_BASIC        0   /* Basic variable (>= 0 indicates row) */
#define VAR_AT_LOWER    -1   /* Nonbasic at lower bound */
#define VAR_AT_UPPER    -2   /* Nonbasic at upper bound */
#define VAR_FREE        -3   /* Free (superbasic) variable */

/* Minimum acceptable weight to avoid division by zero */
#define MIN_WEIGHT 1e-10

/**
 * @brief Select entering variable using steepest edge pricing.
 *
 * Finds the nonbasic variable with the best steepest edge ratio:
 *   SE_ratio = |reduced_cost| / sqrt(weight)
 *
 * This considers both the magnitude of the reduced cost and the
 * length of the edge, leading to better pivot choices and fewer
 * iterations compared to pure Dantzig pricing.
 *
 * Variable attractiveness:
 * - At lower bound (status -1): attractive if RC < -tolerance
 * - At upper bound (status -2): attractive if RC > tolerance
 * - Free variable (status -3): attractive if |RC| > tolerance
 * - Basic (status >= 0): not eligible
 *
 * @param ctx Pricing context (used for statistics)
 * @param reduced_costs Reduced costs array [num_vars]
 * @param weights Steepest edge weights array [num_vars]
 * @param var_status Variable status array [num_vars]
 * @param num_vars Number of variables
 * @param tolerance Optimality tolerance (typically 1e-6)
 * @return Index of entering variable, or -1 if optimal
 */
int cxf_pricing_steepest(PricingContext *ctx, const double *reduced_costs,
                         const double *weights, const int *var_status,
                         int num_vars, double tolerance) {
    /* Validate inputs */
    if (ctx == NULL || reduced_costs == NULL || weights == NULL ||
        var_status == NULL || num_vars <= 0) {
        return -1;
    }

    int best_var = -1;
    double best_ratio = 0.0;
    int candidates_scanned = 0;

    for (int j = 0; j < num_vars; j++) {
        int status = var_status[j];

        /* Skip basic variables */
        if (status >= 0) {
            continue;
        }

        candidates_scanned++;
        double rc = reduced_costs[j];
        double abs_rc = (rc < 0) ? -rc : rc;

        /* Check if variable is attractive based on status */
        int attractive = 0;
        if (status == VAR_AT_LOWER) {
            /* At lower bound: attractive if RC < -tolerance (can increase) */
            if (rc < -tolerance) {
                attractive = 1;
            }
        } else if (status == VAR_AT_UPPER) {
            /* At upper bound: attractive if RC > tolerance (can decrease) */
            if (rc > tolerance) {
                attractive = 1;
            }
        } else if (status == VAR_FREE) {
            /* Free variable: attractive if |RC| > tolerance (can move either way) */
            if (abs_rc > tolerance) {
                attractive = 1;
            }
        }

        if (attractive) {
            /* Get weight, using safeguard for zero/negative weights */
            double weight = weights[j];
            if (weight < MIN_WEIGHT) {
                weight = 1.0;  /* Default weight assumption */
            }

            /* Compute SE ratio: |reduced_cost| / sqrt(weight) */
            double ratio = abs_rc / sqrt(weight);

            if (ratio > best_ratio) {
                best_ratio = ratio;
                best_var = j;
            }
        }
    }

    /* Update statistics if context tracking is enabled */
    if (ctx->total_candidates_scanned >= 0) {
        ctx->total_candidates_scanned += candidates_scanned;
    }

    return best_var;
}

/**
 * @brief Compute steepest edge weight for a single variable.
 *
 * The SE weight gamma_j = ||B^(-1) * a_j||^2 can be expensive to compute
 * from scratch (requires BTRAN), so this is typically only called during
 * initialization or when weights need refresh.
 *
 * Note: This is a helper function. Full weight computation would require
 * basis information (BTRAN), which is passed separately.
 *
 * @param column Basis inverse times column (B^(-1) * a_j) [num_rows]
 * @param num_rows Number of rows (constraints)
 * @return SE weight (squared norm of column)
 */
double cxf_pricing_compute_weight(const double *column, int num_rows) {
    if (column == NULL || num_rows <= 0) {
        return 1.0;  /* Default weight */
    }

    double sum = 0.0;
    for (int i = 0; i < num_rows; i++) {
        sum += column[i] * column[i];
    }
    return sum;
}
