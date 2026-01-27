/**
 * @file locks.c
 * @brief Lock management implementation for ConvexFeld
 *
 * Provides environment-level and solve-level locking primitives.
 * Current implementation provides single-threaded stubs. Actual
 * mutex-based locking will be added when threading is enabled.
 */

#define _POSIX_C_SOURCE 199309L

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

#include <stddef.h>

/**
 * @brief Acquire the environment-level lock
 *
 * In multi-threaded mode, this will acquire a mutex to protect
 * environment-level operations. In single-threaded mode, this is a no-op.
 *
 * @param env Environment to lock (NULL-safe)
 */
void cxf_env_acquire_lock(CxfEnv *env) {
    if (env == NULL) {
        return;  /* NULL-safe: no lock to acquire */
    }

    /* Single-threaded stub: no actual locking yet */
    /* Future: pthread_mutex_lock(&env->mutex) or similar */
}

/**
 * @brief Release the environment-level lock
 *
 * In multi-threaded mode, this will release the environment mutex.
 * In single-threaded mode, this is a no-op.
 *
 * @param env Environment to unlock (NULL-safe)
 */
void cxf_leave_critical_section(CxfEnv *env) {
    if (env == NULL) {
        return;  /* NULL-safe: no lock to release */
    }

    /* Single-threaded stub: no actual locking yet */
    /* Future: pthread_mutex_unlock(&env->mutex) or similar */
}

/**
 * @brief Acquire the solve-level lock
 *
 * In multi-threaded mode, this will acquire a mutex to protect
 * solver state operations. In single-threaded mode, this is a no-op.
 *
 * Note: Using void* for state parameter to avoid circular dependencies.
 * Cast to SolverContext* internally when needed.
 *
 * @param state Solver state to lock (NULL-safe)
 */
void cxf_acquire_solve_lock(void *state) {
    if (state == NULL) {
        return;  /* NULL-safe: no lock to acquire */
    }

    /* Single-threaded stub: no actual locking yet */
    /* Future: pthread_mutex_lock(&solver_context->mutex) or similar */
    /* SolverContext *ctx = (SolverContext *)state; */
}

/**
 * @brief Release the solve-level lock
 *
 * In multi-threaded mode, this will release the solver state mutex.
 * In single-threaded mode, this is a no-op.
 *
 * Note: Using void* for state parameter to avoid circular dependencies.
 * Cast to SolverContext* internally when needed.
 *
 * @param state Solver state to unlock (NULL-safe)
 */
void cxf_release_solve_lock(void *state) {
    if (state == NULL) {
        return;  /* NULL-safe: no lock to release */
    }

    /* Single-threaded stub: no actual locking yet */
    /* Future: pthread_mutex_unlock(&solver_context->mutex) or similar */
    /* SolverContext *ctx = (SolverContext *)state; */
}
