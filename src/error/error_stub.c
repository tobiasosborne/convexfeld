/**
 * @file error_stub.c
 * @brief Stub error handling functions for tracer bullet.
 *
 * Minimal implementation that stores and retrieves error messages.
 * Full implementation with thread safety comes later.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "convexfeld/cxf_env.h"

/**
 * @brief Format and store error message in environment (stub).
 *
 * Formats a printf-style message into the environment's error buffer.
 * This stub version omits thread safety (no critical section).
 *
 * @param env Environment to store error in (NULL is safe)
 * @param format Printf-style format string
 * @param ... Variable arguments for format
 */
void cxf_error(CxfEnv *env, const char *format, ...) {
    if (env == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(env->error_buffer, sizeof(env->error_buffer), format, args);
    va_end(args);

    /* Defensive null termination */
    env->error_buffer[sizeof(env->error_buffer) - 1] = '\0';
}

/**
 * @brief Retrieve last error message from environment (stub).
 *
 * @param env Environment to get error from
 * @return Error message string, or empty string if env is NULL
 */
const char *cxf_geterrormsg(CxfEnv *env) {
    if (env == NULL) {
        return "";
    }
    return env->error_buffer;
}
