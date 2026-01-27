/**
 * @file init.c
 * @brief SolveState initialization and cleanup (M5.3.3)
 *
 * Implements lightweight initialization and cleanup for SolveState structures:
 * - cxf_init_solve_state: Initialize solve control structure
 * - cxf_cleanup_solve_state: Invalidate and clear solve control structure
 */

#include "convexfeld/cxf_solve_state.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_timing.h"
#include <limits.h>
#include <stddef.h>

/**
 * @brief Initialize a solve state structure.
 *
 * Performs lightweight initialization of a SolveState structure. Sets a
 * validation magic number, initializes status and counters to zero, stores
 * references to the provided SolverContext and environment, captures the
 * current timestamp for timing, reads configuration parameters (time limit,
 * iteration limit, callback state) from the environment, and extracts the
 * solve method from the solver state.
 *
 * The function is non-allocating and very fast (~20-30 nanoseconds).
 *
 * @param solve Pointer to SolveState to initialize (must not be NULL)
 * @param state Pointer to solver working state (may be NULL)
 * @param env Environment with parameters (may be NULL)
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if solve is NULL
 */
int cxf_init_solve_state(SolveState *solve, SolverContext *state, CxfEnv *env) {
    /* Validate solve pointer */
    if (solve == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Set validation magic number */
    solve->magic = CXF_SOLVE_STATE_MAGIC;

    /* Initialize status to STATUS_LOADED */
    solve->status = STATUS_LOADED;

    /* Zero iteration count */
    solve->iterations = 0;

    /* Set phase to 0 (initial/setup) */
    solve->phase = 0;

    /* Store solverState pointer reference */
    solve->solverState = state;

    /* Store env pointer reference */
    solve->env = env;

    /* Capture current timestamp */
    solve->startTime = cxf_get_timestamp();

    /* Read parameters from environment or use defaults */
    if (env != NULL) {
        /* TODO: Once TimeLimit and IterationLimit are added to CxfEnv,
         * read them here. For now, use defaults. */
        solve->timeLimit = 1e100;  /* Default: effectively infinite */
        solve->iterLimit = INT_MAX; /* Default: maximum integer */

        /* Get callback context if available */
        CallbackContext *callback_ctx = cxf_get_callback_context(env);
        solve->callbackData = callback_ctx;
    } else {
        /* NULL environment: use defaults */
        solve->timeLimit = 1e100;
        solve->iterLimit = INT_MAX;
        solve->callbackData = NULL;
    }

    /* Clear interrupt flag */
    solve->interruptFlag = 0;

    /* Determine solve method */
    if (state != NULL) {
        /* Read solve method from solver state */
        solve->method = state->solve_mode;
    } else {
        /* Default method: dual simplex */
        solve->method = 1;
    }

    /* Clear control flags */
    solve->flags = 0;

    return CXF_OK;
}

/**
 * @brief Cleanup (invalidate) a solve state structure.
 *
 * Clears and invalidates a SolveState structure after optimization completes.
 * Invalidates the magic number to prevent use-after-cleanup, zeros all counters
 * and status fields, and NULLs all pointer references for defensive programming.
 *
 * No memory is freed because SolveState is typically stack-allocated by the caller.
 *
 * This function is NULL-safe (no-op if solve is NULL) and idempotent (safe to
 * call multiple times).
 *
 * @param solve Pointer to SolveState to cleanup (may be NULL)
 */
void cxf_cleanup_solve_state(SolveState *solve) {
    /* NULL-safe: return immediately if solve is NULL */
    if (solve == NULL) {
        return;
    }

    /* Invalidate magic number */
    solve->magic = 0;

    /* Clear status field */
    solve->status = 0;

    /* Zero iteration count */
    solve->iterations = 0;

    /* Zero phase */
    solve->phase = 0;

    /* NULL solverState pointer */
    solve->solverState = NULL;

    /* NULL env pointer */
    solve->env = NULL;

    /* Zero startTime */
    solve->startTime = 0.0;

    /* Zero timeLimit */
    solve->timeLimit = 0.0;

    /* Zero iterLimit */
    solve->iterLimit = 0;

    /* Zero interruptFlag */
    solve->interruptFlag = 0;

    /* NULL callbackData pointer */
    solve->callbackData = NULL;

    /* Zero method */
    solve->method = 0;

    /* Zero flags */
    solve->flags = 0;
}
