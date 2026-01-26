/**
 * @file terminate.c
 * @brief Termination check function (M3.1.6)
 *
 * Implements cxf_check_terminate for detecting termination requests
 * during optimization loops.
 */

#include "convexfeld/cxf_env.h"
#include <stddef.h>

/**
 * @brief Check if optimization termination has been requested.
 *
 * Examines multiple termination flags in priority order:
 * 1. Direct flag pointer (fastest path for hot loops)
 * 2. Primary environment flag
 *
 * This function is designed for frequent calling (every N iterations)
 * with minimal overhead. Returns 0 for NULL environment (safe default).
 *
 * @param env Environment to check for termination (may be NULL)
 * @return 0 if no termination requested, 1 if termination requested
 */
int cxf_check_terminate(CxfEnv *env) {
    if (env == NULL) {
        return 0;  /* Safe default: continue optimization */
    }

    /* Priority 1: Direct flag pointer (fastest path) */
    if (env->terminate_flag_ptr != NULL) {
        if (*env->terminate_flag_ptr != 0) {
            return 1;  /* Termination detected */
        }
    }

    /* Priority 2: Primary environment flag */
    if (env->terminate_flag != 0) {
        return 1;  /* Termination detected */
    }

    return 0;  /* Continue optimization */
}

/**
 * @brief Request optimization termination.
 *
 * Sets the primary termination flag to signal that optimization
 * should stop gracefully.
 *
 * @param env Environment to terminate (may be NULL)
 */
void cxf_terminate(CxfEnv *env) {
    if (env == NULL) {
        return;
    }
    env->terminate_flag = 1;
}

/**
 * @brief Clear termination request.
 *
 * Resets the termination flag to allow new optimization runs.
 *
 * @param env Environment to reset (may be NULL)
 */
void cxf_clear_terminate(CxfEnv *env) {
    if (env == NULL) {
        return;
    }
    env->terminate_flag = 0;
}
