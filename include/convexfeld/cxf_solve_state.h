/**
 * @file cxf_solve_state.h
 * @brief SolveState structure - lightweight solve control and tracking.
 *
 * SolveState is a small stack-allocated structure that wraps SolverContext
 * and tracks solve progress, manages limits (time, iterations), handles
 * interrupts, and coordinates callbacks. Unlike the heavyweight SolverContext
 * allocation, SolveState performs simple field initialization.
 */

#ifndef CXF_SOLVE_STATE_H
#define CXF_SOLVE_STATE_H

#include "cxf_types.h"
#include <stdint.h>

/*******************************************************************************
 * SolveState Status Constants
 ******************************************************************************/

/** @brief Status: solve state loaded but not started */
#define STATUS_LOADED 1

/*******************************************************************************
 * SolveState Structure
 ******************************************************************************/

/**
 * @brief Lightweight solve control structure.
 *
 * Stack-allocated control structure that wraps SolverContext and manages:
 * - Solve progress tracking (iterations, phase)
 * - Termination conditions (time limit, iteration limit)
 * - Interrupt handling
 * - Callback coordination
 *
 * Size: ~72 bytes
 * Lifetime: Short-lived (duration of one solve call)
 * Allocation: Stack-allocated by caller
 */
typedef struct SolveState {
    uint32_t magic;           /**< Validation magic (0x534f4c56 = "SOLV") */
    int status;               /**< Current status (STATUS_LOADED = 1) */
    int iterations;           /**< Iteration count */
    int phase;                /**< Current phase (0=initial, 1=Phase I, 2=Phase II) */

    /* References */
    SolverContext *solverState; /**< Pointer to solver working state */
    CxfEnv *env;                /**< Environment pointer */

    /* Timing and limits */
    double startTime;         /**< Start timestamp (from cxf_get_timestamp) */
    double timeLimit;         /**< Time limit (from env or 1e100) */
    int iterLimit;            /**< Iteration limit (from env or INT_MAX) */

    /* Control */
    int interruptFlag;        /**< Interrupt flag (0=continue, 1=interrupt) */
    void *callbackData;       /**< Callback data from env */
    int method;               /**< Solve method (from state->solve_mode or default 1=dual simplex) */
    int flags;                /**< Control flags */
} SolveState;

/** @brief Magic number for SolveState validation (0x534f4c56 = "SOLV") */
#define CXF_SOLVE_STATE_MAGIC 0x534f4c56U

/*******************************************************************************
 * SolveState API
 ******************************************************************************/

/**
 * @brief Initialize a solve state structure.
 *
 * Performs lightweight initialization of a SolveState structure:
 * - Sets validation magic number
 * - Initializes status, counters, and phase
 * - Stores references to SolverContext and environment
 * - Captures start timestamp
 * - Reads configuration (time limit, iteration limit, callback state)
 * - Extracts solve method from solver state
 *
 * The solve parameter must point to valid SolveState storage (typically
 * stack-allocated). The state and env parameters may be NULL, in which case
 * default values are used (method=1 for NULL state, infinite limits for NULL env).
 *
 * @param solve Pointer to SolveState to initialize (must not be NULL)
 * @param state Pointer to solver working state (may be NULL)
 * @param env Environment with parameters (may be NULL)
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if solve is NULL
 */
int cxf_init_solve_state(SolveState *solve, SolverContext *state, CxfEnv *env);

/**
 * @brief Cleanup (invalidate) a solve state structure.
 *
 * Clears and invalidates a SolveState structure after optimization completes.
 * Invalidates the magic number and clears all fields to prevent use-after-cleanup.
 * No memory is freed (caller owns the SolveState allocation).
 *
 * This function is NULL-safe and can be called with NULL pointer (no-op).
 * It is safe to call multiple times (idempotent).
 *
 * @param solve Pointer to SolveState to cleanup (may be NULL)
 */
void cxf_cleanup_solve_state(SolveState *solve);

#endif /* CXF_SOLVE_STATE_H */
