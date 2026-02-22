/**
 * @file weight_update.c
 * @brief Steepest edge / Devex weight updates after pivot (P4.9)
 *
 * Implements the simplified one-BTRAN DSE update (Forrest & Goldfarb 1992):
 *   gamma_j' = gamma_j + (tau_j / alpha_{q,r})^2 * (gamma_q - 2*alpha_{q,r} + 1)
 *
 * Devex update (Harris 1973):
 *   gamma_j' = max(eps * gamma_j, (tau_j / alpha_{q,r})^2 + delta_j)
 *
 * Weight for leaving variable:
 *   gamma_p' = gamma_q / alpha_{q,r}^2
 *
 * Spec: revised_simplex.md Step 6
 * Beads: 9ef0
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Minimum weight to prevent division by zero */
#define MIN_WEIGHT      1e-10
/* Devex decay factor (Harris 1973) */
#define DEVEX_DECAY     0.99
/* Strategy constants */
#define STRATEGY_SE     2
#define STRATEGY_DEVEX  3

/**
 * @brief Update steepest edge / Devex weights after a pivot.
 *
 * @param ctx       PricingState with weights array
 * @param state     SolverState with basis and matrix
 * @param entering  Entering variable index
 * @param leavingRow Leaving row index
 * @param pivotCol  FTRAN result alpha_q = B^{-1} a_q [m]
 * @param rho       BTRAN result rho = e_r^T B^{-1} [m] (may be NULL)
 */
void cxf_pricing_update_weights(PricingState *ctx, SolverState *state,
                                int entering, int leavingRow,
                                const double *pivotCol, const double *rho) {
    if (ctx == NULL || state == NULL || ctx->weights == NULL) return;
    if (pivotCol == NULL || leavingRow < 0) return;
    if (ctx->strategy != STRATEGY_SE && ctx->strategy != STRATEGY_DEVEX)
        return;

    BasisState *basis = state->basis;
    if (basis == NULL) return;

    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + 2 * m;
    double pivot_elem = pivotCol[leavingRow];
    if (fabs(pivot_elem) < MIN_WEIGHT) return;

    double pivot_sq = pivot_elem * pivot_elem;
    double gamma_q = ctx->weights[entering];
    if (gamma_q < MIN_WEIGHT) gamma_q = 1.0;

    /* Precompute common factor for DSE simplified update */
    double dse_factor = gamma_q - 2.0 * pivot_elem + 1.0;
    double inv_pivot = 1.0 / pivot_elem;

    /* Update weights for all nonbasic variables */
    for (int j = 0; j < total; j++) {
        if (j == entering) continue;
        if (basis->var_status[j] >= 0) continue;  /* skip basic */
        if (j >= ctx->num_vars) continue;  /* skip if beyond weights array */

        /* Compute tau_j = rho^T a_j (the leaving-row component of B^{-1} a_j) */
        double tau_j = 0.0;
        if (rho != NULL) {
            if (j < n && state->csc_col_ptr != NULL) {
                int64_t cs = state->csc_col_ptr[j];
                int64_t ce = state->csc_col_ptr[j + 1];
                for (int64_t k = cs; k < ce; k++)
                    tau_j += rho[state->csc_row_idx[k]]
                           * state->csc_values[k];
            } else if (j >= n && j - n < m) {
                int row = j - n;
                double coeff = (basis->diag_coeff != NULL)
                    ? basis->diag_coeff[row] : 1.0;
                tau_j = rho[row] * coeff;
            }
        }

        double ratio = tau_j * inv_pivot;

        if (ctx->strategy == STRATEGY_SE) {
            /* Simplified DSE update (one-BTRAN approximation):
             * gamma_j' = gamma_j + ratio^2 * (gamma_q - 2*pivot + 1) */
            double new_w = ctx->weights[j] + ratio * ratio * dse_factor;
            ctx->weights[j] = (new_w > MIN_WEIGHT) ? new_w : MIN_WEIGHT;
        } else {
            /* Devex update (Harris 1973):
             * gamma_j' = max(decay * gamma_j, ratio^2 + delta_j)
             * delta_j = 1 if in reference framework, 0 otherwise.
             * For simplicity, assume all nonbasic in reference (delta=1). */
            double devex_val = ratio * ratio + 1.0;
            double decayed = DEVEX_DECAY * ctx->weights[j];
            ctx->weights[j] = (devex_val > decayed) ? devex_val : decayed;
            if (ctx->weights[j] < MIN_WEIGHT)
                ctx->weights[j] = MIN_WEIGHT;
        }
    }

    /* Weight for leaving variable (now nonbasic):
     * gamma_p' = gamma_q / alpha_{q,r}^2 */
    int leaving = basis->basic_vars[leavingRow];
    if (leaving >= 0 && leaving < ctx->num_vars) {
        double new_w = gamma_q / pivot_sq;
        ctx->weights[leaving] = (new_w > MIN_WEIGHT) ? new_w : MIN_WEIGHT;
    }

    /* Entering variable is now basic — its weight is irrelevant until
     * it leaves the basis, at which point it gets the leaving formula. */
}
