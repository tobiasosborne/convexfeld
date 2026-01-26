/**
 * @file output.c
 * @brief Log output functions (M3.2.2)
 *
 * Provides:
 * - cxf_log_printf: Printf-style log output
 * - cxf_register_log_callback: Register user log callback
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief Printf-style log output.
 *
 * Formats and outputs a log message to configured destinations:
 * - Console (if output_flag enabled)
 * - User callback (if registered)
 *
 * Respects verbosity levels: message only output if verbosity >= level.
 *
 * @param env Environment with logging configuration (NULL is safe)
 * @param level Verbosity level required (0=always, 1=normal, 2+=verbose)
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...) {
    char buffer[1024];
    va_list args;
    int len;

    if (env == NULL || format == NULL) {
        return;
    }

    /* Check verbosity level */
    if (env->verbosity < level) {
        return;
    }

    /* Check output_flag: 0 = suppress all, >= 1 = enable */
    if (env->output_flag <= 0) {
        return;
    }

    /* Format the message */
    va_start(args, format);
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* Defensive null termination */
    buffer[sizeof(buffer) - 1] = '\0';

    /* Handle truncation gracefully (just truncate, no error) */
    (void)len;

    /* Output to console */
    printf("%s\n", buffer);
    fflush(stdout);

    /* Output to user callback if registered */
    if (env->log_callback != NULL) {
        env->log_callback(buffer, env->log_callback_data);
    }
}

/**
 * @brief Register user log callback.
 *
 * Registers a callback function to receive log messages.
 * The callback receives formatted messages (without newline).
 *
 * Pass NULL to unregister the callback.
 *
 * @param env Environment to configure
 * @param callback Callback function, or NULL to unregister
 * @param data User data passed to callback
 * @return CXF_OK on success, error code otherwise
 */
int cxf_register_log_callback(CxfEnv *env,
                               void (*callback)(const char *msg, void *data),
                               void *data) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    env->log_callback = callback;
    env->log_callback_data = data;

    return CXF_OK;
}
