/**
 * @file timestamp.c
 * @brief High-resolution timestamp functions (M4.2.2)
 *
 * Provides monotonic timestamps for measuring elapsed time intervals.
 * Uses clock_gettime(CLOCK_MONOTONIC) on POSIX systems.
 */

/* Enable POSIX features for clock_gettime */
#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Get current high-resolution timestamp.
 *
 * Returns the current monotonic time as a double-precision value
 * representing seconds since an arbitrary epoch. Suitable for
 * measuring elapsed time intervals with microsecond precision.
 *
 * Properties:
 * - Monotonically increasing within a system boot
 * - Not affected by wall-clock adjustments (DST, NTP)
 * - Microsecond precision typical
 *
 * Usage:
 *   double start = cxf_get_timestamp();
 *   // ... work ...
 *   double elapsed = cxf_get_timestamp() - start;
 *
 * @return Current timestamp in seconds (high precision)
 */
double cxf_get_timestamp(void) {
    struct timespec ts;

    /* Use monotonic clock for elapsed time measurement */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /* Extremely rare failure - return 0.0 */
        return 0.0;
    }

    /* Convert to seconds with nanosecond precision */
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
