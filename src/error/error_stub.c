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
#include <math.h>
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

/**
 * @brief Output error message to log destinations (stub).
 *
 * Stub version does nothing - full implementation will write to
 * log file, console, and invoke callbacks.
 *
 * @param env Environment with logging configuration
 * @param message Message to output
 */
void cxf_errorlog(CxfEnv *env, const char *message) {
    (void)env;
    (void)message;
    /* Stub: no output */
}

/**
 * @brief Check array for NaN values.
 *
 * @param arr Array to check
 * @param n Number of elements
 * @return 0 if clean, 1 if NaN found, -1 on error
 */
int cxf_check_nan(const double *arr, int n) {
    if (arr == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (arr[i] != arr[i]) {  /* NaN != NaN */
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Check array for NaN or Infinity values.
 *
 * @param arr Array to check
 * @param n Number of elements
 * @return 0 if all finite, 1 if NaN/Inf found, -1 on error
 */
int cxf_check_nan_or_inf(const double *arr, int n) {
    if (arr == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (!isfinite(arr[i])) {
            return 1;  /* NaN or Inf detected */
        }
    }
    return 0;
}

/**
 * @brief Validate environment pointer and magic number.
 *
 * @param env Environment to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_checkenv(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    return CXF_OK;
}

/**
 * @brief Check if pivot element is numerically acceptable.
 *
 * @param pivot_elem Pivot element value
 * @param tolerance Minimum acceptable magnitude
 * @return 1 if valid, 0 if invalid (too small, NaN, etc.)
 */
int cxf_pivot_check(double pivot_elem, double tolerance) {
    /* Check for NaN */
    if (pivot_elem != pivot_elem) {
        return 0;
    }
    /* Check magnitude */
    double abs_val = (pivot_elem < 0) ? -pivot_elem : pivot_elem;
    if (abs_val < tolerance) {
        return 0;
    }
    return 1;
}
