/**
 * @file cxf_pricing.h
 * @brief PricingContext structure - partial pricing state.
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
struct PricingContext {
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
};

#endif /* CXF_PRICING_H */
