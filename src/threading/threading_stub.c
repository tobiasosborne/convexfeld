/**
 * @file threading_stub.c
 * @brief Stub implementations for threading functions (M3.3.1)
 *
 * TDD stub implementations for threading module.
 * These provide minimal functionality to make tests pass.
 * Full implementations will be added in later milestones.
 */

/* For sysconf on POSIX systems, QueryPerformanceCounter on Windows */
#define _POSIX_C_SOURCE 199309L

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#endif

/* cxf_get_logical_processors is already implemented in logging/system.c */

/**
 * @brief Get the number of physical CPU cores.
 *
 * Detects physical cores excluding hyperthreads.
 * Falls back to logical processor count if physical detection fails.
 *
 * @return Number of physical cores (minimum 1)
 */
int cxf_get_physical_cores(void) {
    /* Stub: return logical processors as fallback per spec */
    int cxf_get_logical_processors(void);
    return cxf_get_logical_processors();
}

/**
 * @brief Set the thread count for the solver.
 *
 * Validates and stores the thread count. Caps at logical processor count.
 *
 * @param env Environment to configure
 * @param thread_count Number of threads (must be >= 1)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_set_thread_count(CxfEnv *env, int thread_count) {
    if (env == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    if (thread_count < 1) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    /* Stub: accept any positive count (will be capped later) */
    (void)thread_count;  /* Not stored yet */
    return CXF_OK;
}

/**
 * @brief Get the configured thread count.
 *
 * Returns the Threads parameter value.
 * Returns 0 for NULL env or if parameter not set.
 *
 * @param env Environment to query
 * @return Thread count (0 = auto, >0 = specific count)
 */
int cxf_get_threads(CxfEnv *env) {
    if (env == NULL) {
        return 0;
    }
    /* Stub: return 0 (auto mode) */
    return 0;
}

/**
 * @brief Acquire the environment-level lock.
 *
 * NULL-safe: does nothing if env is NULL.
 *
 * @param env Environment containing the lock
 */
void cxf_env_acquire_lock(CxfEnv *env) {
    if (env == NULL) {
        return;
    }
    /* Stub: no-op (single-threaded for now) */
}

/**
 * @brief Release the environment-level lock.
 *
 * NULL-safe: does nothing if env is NULL.
 *
 * @param env Environment containing the lock
 */
void cxf_leave_critical_section(CxfEnv *env) {
    if (env == NULL) {
        return;
    }
    /* Stub: no-op (single-threaded for now) */
}

/**
 * @brief Generate a pseudo-random seed.
 *
 * Combines timestamp, process ID, and thread ID for entropy.
 * Result is always non-negative.
 *
 * @return Non-negative seed value
 */
int cxf_generate_seed(void) {
    unsigned int seed = 0;

#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    seed = (unsigned int)(counter.QuadPart ^ (counter.QuadPart >> 32));
    seed ^= (unsigned int)GetCurrentProcessId();
    seed ^= (unsigned int)GetCurrentThreadId();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    seed = (unsigned int)ts.tv_nsec;
    seed ^= (unsigned int)ts.tv_sec;
    seed ^= (unsigned int)getpid();
#endif

    /* Simple hash mixing for better distribution */
    seed ^= (seed >> 16);
    seed *= 0x85ebca6b;
    seed ^= (seed >> 13);
    seed *= 0xc2b2ae35;
    seed ^= (seed >> 16);

    /* Ensure non-negative */
    return (int)(seed & 0x7FFFFFFF);
}
