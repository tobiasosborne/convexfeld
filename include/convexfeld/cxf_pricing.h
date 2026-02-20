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

/**
 * @brief Pricing context for partial pricing.
 *
 * Maintains a hierarchy of candidate subsets for efficient pricing.
 * Starts with small candidate sets and expands only when necessary.
 */
struct PricingState {
    int current_level;        /**< Active pricing level (0=full) */
    int max_levels;           /**< Number of levels (typically 3-5) */

    /* Problem dimensions */
    int num_vars;             /**< Number of variables in the problem */
    int strategy;             /**< Pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex) */

    /* Candidate arrays per level */
    int *candidate_counts;    /**< Candidates at each level [max_levels] */
    int **candidate_arrays;   /**< Variable indices per level [max_levels] */
    int *candidate_sizes;     /**< Allocated size per level [max_levels] */

    /* Steepest edge weights */
    double *weights;          /**< SE/Devex weights [num_vars], NULL if unused */

    /* Cache */
    int *cached_counts;       /**< Cached result count (-1=invalid) [max_levels] */

    /* Statistics */
    int last_pivot_iteration; /**< Iteration of last pivot */
    int64_t total_candidates_scanned; /**< Cumulative candidates evaluated */
    int level_escalations;    /**< Count of level increases */

    /* V2: Dirty flags for incremental pricing (F1) */
    int *var_dirty;           /**< Per-variable dirty flag [num_vars] */
    int num_dirty;            /**< Count of dirty variables */

    /* V2: Constraint queues (F1) */
    int num_constrs;          /**< Number of constraints */
    int *constr_dirty;        /**< Per-constraint dirty flag [num_constrs] */
    int num_constr_dirty;     /**< Count of dirty constraints */
    int *constr_candidates;   /**< Constraint candidate list [num_constrs] */
    int num_constr_candidates;/**< Count of constraint candidates */
};

/*******************************************************************************
 * V2 Pricing Queue API (F1)
 ******************************************************************************/

/** Mark a variable as needing repricing after bound/status change. */
void cxf_pricing_mark_dirty(PricingState *ctx, int var_idx);

/** Mark a constraint as needing propagation evaluation. */
void cxf_pricing_mark_constr_dirty(PricingState *ctx, int constr_idx);

/** Cascade dirty marks along a column (after pivot/bound change).
 *  Marks the variable dirty and all constraints in its CSC column. */
void cxf_pricing_cascade_update(PricingState *ctx,
                                struct SolverState *state, int var_idx);

/** Complete current pricing level (promote/demote candidates). */
void cxf_pricing_end_level(PricingState *ctx);

/** Set active pricing level. */
void cxf_pricing_set_level(PricingState *ctx, int level);

/** Get constraint candidates for phase_end/step3 processing. */
int cxf_pricing_get_constr_candidates(PricingState *ctx, int *out,
                                      int max_out);

#endif /* CXF_PRICING_H */
