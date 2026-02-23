/**
 * @file format.c
 * @brief Format helper functions for logging (M3.2.3)
 *
 * Provides safe wrappers for formatting operations:
 * - cxf_snprintf_wrapper: safe printf-style formatting
 */

#include "convexfeld/cxf_types.h"
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief Safe wrapper for snprintf.
 *
 * Provides consistent behavior across platforms and
 * ensures null termination.
 *
 * @param buffer Output buffer (must not be NULL)
 * @param size Buffer size in bytes (must be > 0)
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return Number of characters that would be written (excluding null),
 *         or -1 if buffer is NULL or size is 0
 */
int cxf_snprintf_wrapper(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    int result;

    /* Validate inputs per specification */
    if (buffer == NULL || size == 0) {
        return -1;
    }

    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);

    /* Ensure null termination for safety */
    buffer[size - 1] = '\0';

    return result;
}
