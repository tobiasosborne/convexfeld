/**
 * @file cxf_pricing.h
 * @brief PricingState structure - partial pricing state.
 *
 * Implements multi-level partial pricing for efficient
 * entering variable selection in the simplex method.
 */

#ifndef CXF_PRICING_H
#define CXF_PRICING_H

#include "cxf_types.h"
#include <stdint.h>

/** Maximum number of pricing levels (v2 spec: 3 = base + 2 neighborhoods) */
#define CXF_MAX_PRICING_LEVELS 3

/**
 * @brief Pricing context for multi-level partial pricing (v2).
 *
 * Maintains per-level variable and constraint queues with committed/pending
 * split, 4-bit membership flags, and 3-slot caches per level.
 * Spec: pricing_state.md
 */
struct PricingState {
    int current_level;        /**< Active pricing level (0=base) */
    int max_levels;           /**< Number of levels (typically 3) */

    /* Problem dimensions */
    int num_vars;             /**< Number of variables in the problem */
    int num_constrs;          /**< Number of constraints */
    int strategy;             /**< Pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex) */

    /* V1 candidate arrays (retained for backward compat until P4.5 rewrites) */
    int *candidate_counts;    /**< Candidates at each level [max_levels] */
    int **candidate_arrays;   /**< Variable indices per level [max_levels] */
    int *candidate_sizes;     /**< Allocated size per level [max_levels] */

    /* Steepest edge weights */
    double *weights;          /**< SE/Devex weights [num_vars], NULL if unused */

    /* V1 cache (retained until P4.5) */
    int *cached_counts;       /**< Cached result count (-1=invalid) [max_levels] */

    /* Statistics */
    int last_pivot_iteration; /**< Iteration of last pivot */
    int64_t total_candidates_scanned; /**< Cumulative candidates evaluated */
    int level_escalations;    /**< Count of level increases */

    /* V1 dirty flags (retained until P4.7 rewrites mark_dirty) */
    int *var_dirty;           /**< Per-variable dirty flag [num_vars] */
    int num_dirty;            /**< Count of dirty variables */
    int *constr_dirty;        /**< Per-constraint dirty flag [num_constrs] */
    int num_constr_dirty;     /**< Count of dirty constraints */
    int *constr_candidates;   /**< Constraint candidate list [num_constrs] */
    int num_constr_candidates;/**< Count of constraint candidates */

    /* ========== V2 multi-level queue system (P4.1) ========== */

    /* Level management */
    int level_active[CXF_MAX_PRICING_LEVELS]; /**< Per-level activation flag */

    /* Variable queue system — 4-bit flags + per-level queues */
    uint8_t *var_flags;       /**< 4-bit membership flags [num_vars] */
    int var_q_committed[CXF_MAX_PRICING_LEVELS]; /**< Committed count per level */
    int var_q_total[CXF_MAX_PRICING_LEVELS];     /**< Total count per level */
    int *var_queue[CXF_MAX_PRICING_LEVELS];      /**< Queue arrays per level */
    int var_queue_cap[CXF_MAX_PRICING_LEVELS];   /**< Allocated capacity per level */
    int cached_var_count[CXF_MAX_PRICING_LEVELS];  /**< Primary cache slot */
    int cached_var_count2[CXF_MAX_PRICING_LEVELS]; /**< Secondary cache slot */
    int cached_var_count3[CXF_MAX_PRICING_LEVELS]; /**< Tertiary cache slot */
    int *var_output_buf[CXF_MAX_PRICING_LEVELS];   /**< Output buffers per level */

    /* Constraint queue system — 4-bit flags + per-level queues */
    uint8_t *constr_flags;    /**< 4-bit membership flags [num_constrs] */
    int constr_q_committed[CXF_MAX_PRICING_LEVELS]; /**< Committed count per level */
    int constr_q_total[CXF_MAX_PRICING_LEVELS];     /**< Total count per level */
    int *constr_queue[CXF_MAX_PRICING_LEVELS];       /**< Queue arrays per level */
    int constr_queue_cap[CXF_MAX_PRICING_LEVELS];    /**< Allocated capacity */
    int cached_constr_count[CXF_MAX_PRICING_LEVELS];  /**< Primary cache slot */
    int cached_constr_count2[CXF_MAX_PRICING_LEVELS]; /**< Secondary cache slot */
    int cached_constr_count3[CXF_MAX_PRICING_LEVELS]; /**< Tertiary cache slot */
    int *constr_output_buf[CXF_MAX_PRICING_LEVELS];   /**< Output buffers */

    /* Devex reference framework (revised_simplex.md Step 6) */
    uint8_t *ref_framework;       /**< Per-variable: 1 = in R, 0 = not [num_vars] */
    int ref_framework_count;      /**< Vars still in R */
    int ref_framework_initial;    /**< Original R size (staleness check) */
};

/*******************************************************************************
 * V2 Pricing Queue API (F1)
 ******************************************************************************/

/** Mark a single variable dirty in both L1 and L2 queues (V2 spec). */
void cxf_pricing_invalidate(PricingState *ctx, int varIndex);

/** Reset cached candidates/weights (cache management, not dirty marking). */
void cxf_pricing_invalidate_cache(PricingState *ctx, int flags);

/** Mark a variable as needing repricing after bound/status change. */
void cxf_pricing_mark_dirty(PricingState *ctx, int var_idx);

/** Mark a constraint as needing propagation evaluation. */
void cxf_pricing_mark_constr_dirty(PricingState *ctx, int constr_idx);

/** Cascade dirty marks along a column (after pivot/bound change).
 *  Marks the variable dirty and all constraints in its CSC column. */
void cxf_pricing_cascade_update(PricingState *ctx,
                                struct SolverState *state, int var_idx);

/** Complete current pricing level (promote/demote candidates).
 *  V2 spec: filters queues using status arrays from SolverState.
 *  @param state May be NULL (skips queue filtering, only does cache work). */
void cxf_pricing_end_level(PricingState *ctx, struct SolverState *state);

/** Set active pricing level. */
void cxf_pricing_set_level(PricingState *ctx, int level);

/** Get constraint candidates for phase_end/step3 processing. */
int cxf_pricing_get_constr_candidates(PricingState *ctx, int *out,
                                      int max_out);

/*******************************************************************************
 * V2 Pricing Core API (P4.2-P4.5)
 ******************************************************************************/

/** V2 queue insertion helpers (queue_insert.c). */
void v2_insert_constr(PricingState *ctx, int constr_idx);
void v2_insert_var(PricingState *ctx, int var_idx);

/** P4.2: Mark constraints dirty after variable enters basis (CSC traversal). */
void cxf_pricing_update_var(PricingState *ctx,
                            struct SolverState *state, int varIndex);

/** P4.3: Mark variables dirty after constraint leaves basis (CSR traversal). */
void cxf_pricing_update_constr(PricingState *ctx,
                               struct SolverState *state, int constrIndex);

/** P4.4: Process V2 queues at current level (filter, promote, invalidate). */
void cxf_pricing_update(PricingState *ctx,
                               struct SolverState *state);

/** P4.5: Retrieve candidate variables via V2 adaptive strategy. */
void cxf_pricing_candidates(PricingState *ctx,
                            struct SolverState *state,
                            int *count_out, int **candidates_out);

/** P3.18: Get committed constraint stats at current level. */
void cxf_pricing_get_constr_stats(PricingState *ctx, int *count_out,
                                  int **queue_out);

/** P3.18: V2 adaptive constraint candidate retrieval. */
void cxf_pricing_constr_candidates_v2(PricingState *ctx,
                                      struct SolverState *state,
                                      int *count_out, int **candidates_out);

/** Recompute SE/Devex weights from scratch (at refactorization). */
void cxf_pricing_recompute_weights(PricingState *ctx,
                                    struct SolverState *state);

/** P4.9: Update steepest edge / Devex weights after pivot. */
void cxf_pricing_update_weights(PricingState *ctx,
                                struct SolverState *state,
                                int entering, int leavingRow,
                                const double *pivotCol, const double *rho);

#endif /* CXF_PRICING_H */
