/**
 * @file config.c
 * @brief Thread configuration: effective thread count resolution
 *
 * Implements cxf_get_threads and cxf_set_thread_count per
 * threading_sync.md V2 spec.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* From logging/system.c */
int cxf_get_logical_processors(void);
/* From threading/cpu.c */
int cxf_get_physical_cores(void);
/* From logging/output.c */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);

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
 * @brief Validate a requested thread count per V2 threading_sync.md.
 *
 * If thread_count exceeds logical processors, emits a warning via
 * cxf_log_printf. Does NOT store the value or modify any state.
 * Always succeeds (purely diagnostic).
 *
 * @param env           Environment for log output and hardware info
 * @param thread_count  Thread count to validate
 */
void cxf_set_thread_count(CxfEnv *env, int thread_count) {
    int logical;

    if (env == NULL) {
        return;
    }

    logical = cxf_get_logical_processors();

    if (thread_count > logical) {
        cxf_log_printf(env, 0, "--------------------------------------");
        cxf_log_printf(env, 0,
            "Warning: Threads (%d) exceeds logical processors (%d)",
            thread_count, logical);
        cxf_log_printf(env, 0,
            "Consider reducing the Threads parameter to avoid "
            "oversubscription");
        cxf_log_printf(env, 0, "--------------------------------------");
    }
}
