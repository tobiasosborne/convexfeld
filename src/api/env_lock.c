/**
 * @file env_lock.c
 * @brief cxf_env_acquire_lock - clear error buffer for new API operation.
 *
 * Per V2 threading_sync.md: clears the error buffer state in preparation
 * for a new API operation, unless the error buffer is locked for nested
 * error handling. Despite the name, no mutex is acquired.
 */

#include "convexfeld/cxf_env.h"

#include <stddef.h>

void cxf_env_acquire_lock(CxfEnv *env) {
    if (env == NULL) {
        return;  /* Null environment -> silent return */
    }

    if (env->error_buf_locked) {
        return;  /* Buffer locked for nested error handling -> preserve state */
    }

    /* Clear error state for new API operation */
    env->error_code = 0;
    env->error_buffer[0] = '\0';
}
