/**
 * @file config.c
 * @brief Thread configuration: effective thread count resolution
 *
 * Implements cxf_get_threads per threading_sync.md V2 spec:
 * hierarchy of constraints, most restrictive wins.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* From logging/system.c */
int cxf_get_logical_processors(void);
/* From threading/cpu.c */
int cxf_get_physical_cores(void);

/** @brief Internal cap on auto-detected thread count (~32 per spec) */
#define CXF_THREAD_CAP 32

/**
 * @brief Compute effective thread count per V2 threading_sync.md.
 *
 * Resolution chain (each step can only reduce, never increase):
 *  1. Model-level override (not yet wired; reserved for future use)
 *  2. Auto-detect: logical processors, prefer physical cores on large
 *     systems, cap at CXF_THREAD_CAP
 *  3. User Threads parameter (env->threads): if > 0 and < current, use it
 *  4. License thread limit (env->license_thread_limit): if > 0 and < current
 *
 * @param env Environment (may be NULL)
 * @return Positive effective thread count, or 0 if env is NULL
 */
int cxf_get_threads(CxfEnv *env) {
    int count;
    int physical;

    if (env == NULL) {
        return 0;
    }

    /* Step 1: model-level override (reserved, not yet wired) */
    /* When CxfModel gains a thread_override field, check it here:
     *   if (model_thread_override >= 1) count = model_thread_override;
     * For now, fall through to auto-detection. */

    /* Step 2: auto-detect with cap */
    count = cxf_get_logical_processors();

    if (count > CXF_THREAD_CAP) {
        /* Prefer physical cores on large systems */
        physical = cxf_get_physical_cores();
        if (physical < count) {
            count = physical;
        }
        /* Clamp to cap */
        if (count > CXF_THREAD_CAP) {
            count = CXF_THREAD_CAP;
        }
    }

    /* Step 3: user Threads parameter (0 = auto, no reduction) */
    if (env->threads > 0 && env->threads < count) {
        count = env->threads;
    }

    /* Step 4: license thread limit (0 = unlimited, no reduction) */
    if (env->license_thread_limit > 0 && env->license_thread_limit < count) {
        count = env->license_thread_limit;
    }

    /* Ensure at least 1 */
    return (count > 0) ? count : 1;
}

/**
 * @brief Validate a requested thread count.
 *
 * Checks that thread_count >= 1. Does NOT store the value;
 * storage is via cxf_setintparam("Threads", N).
 *
 * @param env  Environment handle (must not be NULL)
 * @param thread_count  Thread count to validate (must be >= 1)
 * @return CXF_OK on success, CXF_ERROR_INVALID_ARGUMENT otherwise
 */
int cxf_validate_thread_count(CxfEnv *env, int thread_count) {
    if (env == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    if (thread_count < 1) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    return CXF_OK;
}
