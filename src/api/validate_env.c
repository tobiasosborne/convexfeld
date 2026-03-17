/**
 * @file validate_env.c
 * @brief Environment pointer validation per V2 input_validation.md cxf_check_env.
 *
 * Two-level sentinel check: validates both the environment's own magic number
 * and the root (master) environment's magic number. Guards against direct
 * use-after-free and indirect corruption where a child outlives its root.
 */

#include "convexfeld/cxf_env.h"
#include <stdio.h>

/* Logging subsystem (for optional warning output) */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);

/**
 * @brief Validate that an environment pointer is non-null, live, and
 *        structurally consistent.
 *
 * Per V2 input_validation.md cxf_check_env:
 * 1. Check env is non-null -> CXF_ERROR_NULL_ARGUMENT if null
 * 2. Read validation sentinel, compare against CXF_ENV_MAGIC
 *    -> CXF_ERROR_INVALID_ARGUMENT on mismatch
 * 3. Follow root environment pointer (master_env), check root's sentinel
 *    -> CXF_ERROR_INVALID_ARGUMENT on mismatch (with optional warning
 *       if output verbosity > 0)
 * 4. Both sentinels must match for success (return 0)
 *
 * Thread safety: safe (read-only access to immutable-after-init sentinel fields).
 *
 * @param env  The environment pointer to validate
 * @return CXF_OK (0) on success, or an error code
 */
int cxf_check_env(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Primary sentinel check */
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Root environment sentinel check.
     * master_env is NULL for root environments (self-rooted).
     * For child environments, follow the pointer and verify. */
    if (env->master_env != NULL) {
        if (env->master_env->magic != CXF_ENV_MAGIC) {
            /* Optional warning gated by output verbosity */
            if (env->output_flag > 0 && env->verbosity > 0) {
                cxf_log_printf(env, 0,
                    "Warning: root environment may have been freed prematurely");
            }
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    return CXF_OK;
}
