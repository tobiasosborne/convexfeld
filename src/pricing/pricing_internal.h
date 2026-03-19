/**
 * @file pricing_internal.h
 * @brief Internal declarations for cross-file pricing module functions.
 *
 * Replaces scattered extern declarations with a single shared header.
 * Also centralizes shared constants that were duplicated across files.
 */

#ifndef PRICING_INTERNAL_H
#define PRICING_INTERNAL_H

/* Forward declarations */
struct PricingState;
struct SolverState;

/* --- Lifecycle (context.c) --- */
void cxf_pricing_free(struct PricingState *ctx);

/* --- Candidate selection (candidates.c) --- */
int cxf_pricing_candidates_v1(struct PricingState *ctx, const double *rc,
                              const int *vs, int nv, double tol,
                              int *out, int max_out);

/* --- Invalidation (invalidate.c) --- */
void cxf_pricing_invalidate(struct PricingState *ctx, int varIndex);
void cxf_pricing_invalidate_cache(struct PricingState *ctx, int flags);

/* --- Queue insertion (queue_insert.c) --- */
void v2_insert_var(struct PricingState *ctx, int var_idx);
void v2_insert_constr(struct PricingState *ctx, int constr_idx);

/* Invalidation flags */
#define CXF_INVALID_CANDIDATES     0x01
#define CXF_INVALID_REDUCED_COSTS  0x02
#define CXF_INVALID_WEIGHTS        0x04
#define CXF_INVALID_ALL            0xFF

/* Queue flag bits (shared between queue_insert.c and update.c) */
#define L1_COMMITTED 0x01
#define L1_PENDING   0x02
#define L1_MASK      0x03
#define L2_COMMITTED 0x04
#define L2_PENDING   0x08
#define L2_MASK      0x0C

/* Pricing exclusion flag (T2.2: perturbation removes degenerate vars) */
#define PRICING_EXCLUDED 0x20

/* Minimum weight for SE/Devex (shared between steepest.c and weight_update.c) */
#define CXF_MIN_WEIGHT 1e-10

/* Strategy constants */
#define STRATEGY_AUTO    0
#define STRATEGY_PARTIAL 1
#define STRATEGY_SE      2
#define STRATEGY_DEVEX   3

/* Variable status aliases (match CXF_VAR_* from cxf_types.h) */
#define VAR_BASIC     0
#define VAR_AT_LOWER  (-1)
#define VAR_AT_UPPER  (-2)
#define VAR_FREE      (-3)

#endif /* PRICING_INTERNAL_H */
