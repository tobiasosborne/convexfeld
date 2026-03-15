/**
 * @file timestamp.c
 * @brief Elapsed time and session-ID timestamp functions (M4.2.2)
 *
 * Two distinct functions:
 * - cxf_get_elapsed_time: monotonic clock for timing intervals
 * - cxf_get_timestamp: V2 spec session-ID generator (int64_t)
 */

/* Enable POSIX features for clock_gettime */
#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include <stdint.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Get current monotonic time for elapsed-time measurement.
 *
 * Returns seconds since an arbitrary epoch (CLOCK_MONOTONIC).
 * NOT affected by wall-clock adjustments (DST, NTP).
 *
 * @return Current monotonic time in seconds (high precision)
 */
double cxf_get_elapsed_time(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/**
 * @brief Generate a 64-bit session identifier per V2 spec.
 *
 * Three-step process (statistics_diagnostics.md):
 *   1. Acquire UTC system time via CLOCK_REALTIME
 *   2. Convert to nanoseconds since Unix epoch
 *   3. Multiply by large odd constant for hash mixing
 *
 * The result is NOT a readable timestamp. It is a hashed
 * session ID for logging, correlation, and registration.
 *
 * @return 64-bit hashed session identifier
 */
int64_t cxf_get_timestamp(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }

    /* Nanoseconds since Unix epoch */
    int64_t nanos = (int64_t)ts.tv_sec * (int64_t)1000000000
                  + (int64_t)ts.tv_nsec;

    /* Hash mixing: multiply by large odd constant */
    int64_t mixed = nanos * (int64_t)0x517CC1B727220A95LL;

    return mixed;
}
