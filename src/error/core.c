/**
 * @file core.c
 * @brief Core error functions implementation (M3.1.2)
 *
 * Implements cxf_error and cxf_errorlog with enhanced functionality.
 * Thread safety features deferred until critical section infrastructure exists.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "convexfeld/cxf_env.h"

/**
 * @brief Format and store error message in environment.
 *
 * Formats a printf-style message into the environment's error buffer.
 * Thread safety (critical section) deferred until infrastructure exists.
 * errorBufLocked check deferred until field is added to CxfEnv.
 *
 * @param env Environment to store error in (NULL is safe)
 * @param format Printf-style format string
 * @param ... Variable arguments for format
 */
void cxf_error(CxfEnv *env, const char *format, ...) {
    if (env == NULL) {
        return;
    }

    /* Note: errorBufLocked check would go here if field existed */
    /* Note: Critical section acquire would go here if available */

    va_list args;
    va_start(args, format);
    vsnprintf(env->error_buffer, sizeof(env->error_buffer), format, args);
    va_end(args);

    /* Defensive null termination */
    env->error_buffer[sizeof(env->error_buffer) - 1] = '\0';

    /* Note: Critical section release would go here if available */
}

/**
 * @brief Retrieve last error message from environment.
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

/**
 * @brief Output message to configured log destinations.
 *
 * Writes message to console based on output_flag setting.
 * Log file and callback support deferred until CxfEnv has those fields.
 *
 * @param env Environment with logging configuration
 * @param message Message to output (NULL is safe)
 */
void cxf_errorlog(CxfEnv *env, const char *message) {
    if (env == NULL || message == NULL) {
        return;
    }

    /* Check output_flag: 0 = suppress, >= 1 = enable */
    if (env->output_flag <= 0) {
        return;
    }

    /* Note: Critical section acquire would go here if available */

    /* Note: Log file output would go here if CxfEnv had log file handle */

    /* Console output */
    printf("%s\n", message);
    fflush(stdout);

    /* Note: Log callback would go here if CxfEnv had callback pointer */

    /* Clear error buffer if it matches the message we just logged */
    if (strcmp(env->error_buffer, message) == 0) {
        env->error_buffer[0] = '\0';
    }

    /* Note: Critical section release would go here if available */
}
