/**
 * @file config.c
 * @brief Thread configuration implementation
 *
 * Provides functions for configuring thread count in the environment.
 * Currently implements stub behavior where thread count is not stored,
 * and auto-mode (0 threads) is always returned.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/**
 * @brief Get the configured thread count
 *
 * Returns the number of threads configured for parallel operations.
 * Currently returns 0 (auto mode) for all non-NULL environments.
 *
 * @param env Environment handle (may be NULL)
 * @return Number of threads configured, or 0 for auto mode or NULL env
 *
 * @note Current stub implementation:
 *       - Returns 0 for NULL env
 *       - Returns 0 (auto mode) for valid env
 *       - Thread count storage not yet implemented
 */
int cxf_get_threads(CxfEnv *env) {
    if (env == NULL) {
        return 0;
    }

    /* Stub: Always return 0 (auto mode) until thread count storage
     * is added to CxfEnv structure */
    return 0;
}

/**
 * @brief Set the thread count for parallel operations
 *
 * Configures the number of threads to use for parallel operations.
 * Currently validates input but does not store the value (stub behavior).
 *
 * @param env Environment handle (must not be NULL)
 * @param thread_count Number of threads to use (must be >= 1)
 * @return CXF_OK on success
 * @return CXF_ERROR_INVALID_ARGUMENT if env is NULL
 * @return CXF_ERROR_INVALID_ARGUMENT if thread_count < 1
 *
 * @note Current stub implementation:
 *       - Validates parameters
 *       - Does not store thread_count (storage not in CxfEnv yet)
 *       - Returns CXF_OK for valid inputs
 *
 * @note Future implementation will:
 *       - Store thread_count in CxfEnv structure
 *       - Allow retrieval via cxf_get_threads()
 */
int cxf_set_thread_count(CxfEnv *env, int thread_count) {
    /* Validate environment handle */
    if (env == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate thread count - must be at least 1 */
    if (thread_count < 1) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Stub: Accept the thread_count but don't store it yet
     * Thread count storage will be added to CxfEnv in future milestone */
    (void)thread_count;

    return CXF_OK;
}
