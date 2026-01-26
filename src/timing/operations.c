/**
 * @file operations.c
 * @brief Operation timing functions (M4.2.4)
 *
 * Implements timing functions for specific solver operations:
 * - cxf_timing_pivot: Record work from simplex pivot operations
 * - cxf_timing_refactor: Determine if refactorization is needed
 */

#include "convexfeld/cxf_timing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include <stddef.h>

/**
 * @brief Record computational work from a simplex pivot operation.
 *
 * Accumulates work metrics from the three main phases of a simplex pivot:
 * pricing (entering variable), ratio test (leaving variable), and basis
 * update (eta vector creation). The work is scaled and accumulated for
 * refactorization decision making.
 *
 * @param state Solver state with timing/work tracking (may be NULL)
 * @param pricing_work Work units spent in pricing phase (>= 0)
 * @param ratio_work Work units spent in ratio test phase (>= 0)
 * @param update_work Work units spent in basis update phase (>= 0)
 */
void cxf_timing_pivot(SolverContext *state,
                      double pricing_work,
                      double ratio_work,
                      double update_work) {
    if (state == NULL) {
        return;
    }

    /* Update work counter if enabled */
    if (state->work_counter != NULL) {
        double total_work = pricing_work + ratio_work + update_work;
        double scaled_work = total_work * state->scale_factor;
        *state->work_counter += scaled_work;
    }

    /* Update timing state if enabled */
    if (state->timing != NULL) {
        /* Category indices for different phases */
        enum {
            CAT_TOTAL   = 0,
            CAT_PRICING = 1,
            CAT_RATIO   = 2,
            CAT_UPDATE  = 3
        };

        /* Accumulate phase-specific timing */
        state->timing->total_time[CAT_PRICING] += pricing_work;
        state->timing->total_time[CAT_RATIO] += ratio_work;
        state->timing->total_time[CAT_UPDATE] += update_work;

        /* Increment operation counts */
        state->timing->operation_count[CAT_PRICING]++;
        state->timing->operation_count[CAT_RATIO]++;
        state->timing->operation_count[CAT_UPDATE]++;
        state->timing->operation_count[CAT_TOTAL]++;
    }
}

/**
 * @brief Determine if basis refactorization should be triggered.
 *
 * Evaluates multiple criteria to decide if refactorization is needed:
 * - Hard limits: eta count, eta memory (returns 2 = required)
 * - Soft criteria: FTRAN time degradation, iteration count (returns 1 = recommended)
 *
 * @param state Solver state with refactorization tracking (may be NULL)
 * @param env Environment with refactorization parameters (may be NULL)
 * @return 0 = not needed, 1 = recommended, 2 = required
 */
int cxf_timing_refactor(SolverContext *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return 0;  /* Cannot evaluate, assume not needed */
    }

    /* Check hard limits */
    if (env->max_eta_count > 0 && state->eta_count > env->max_eta_count) {
        return 2;  /* Required: exceeded max eta count */
    }

    if (env->max_eta_memory > 0 && state->eta_memory > env->max_eta_memory) {
        return 2;  /* Required: exceeded max eta memory */
    }

    /* Check performance degradation (FTRAN time) */
    if (state->ftran_count > 0 && state->baseline_ftran > 0.0) {
        double avg_ftran = state->total_ftran_time / (double)state->ftran_count;
        /* Recommend refactor if FTRAN is 3x slower than baseline */
        if (avg_ftran > state->baseline_ftran * 3.0) {
            return 1;  /* Recommended: FTRAN degradation */
        }
    }

    /* Check iteration count */
    if (env->refactor_interval > 0) {
        int iters_since = state->iteration - state->last_refactor_iter;
        if (iters_since > env->refactor_interval) {
            return 1;  /* Recommended: exceeded refactor interval */
        }
    }

    return 0;  /* Not needed */
}
