/**
 * @file seed.c
 * @brief Pseudo-random seed generation for thread-safe random number generation
 *
 * Generates seeds by combining high-resolution timestamps, process IDs, and
 * thread IDs with hash mixing for better distribution. Platform-specific
 * implementations ensure consistent behavior across Windows and POSIX systems.
 */

#define _POSIX_C_SOURCE 199309L
#include "convexfeld/cxf_types.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#endif

/**
 * @brief Generate a pseudo-random seed value
 *
 * Creates a seed by combining multiple entropy sources:
 * - High-resolution timestamp (nanosecond precision on POSIX)
 * - Process ID
 * - Thread ID (Windows only)
 *
 * The combined value is then mixed using a hash function to improve
 * distribution and reduce correlation between similar inputs.
 *
 * @return Non-negative seed value suitable for seeding random number generators
 *
 * @note Thread-safe: Each call produces a unique seed based on timing
 * @note Platform-specific: Uses QueryPerformanceCounter on Windows,
 *       clock_gettime(CLOCK_MONOTONIC) on POSIX systems
 */
int cxf_generate_seed(void) {
    unsigned int seed = 0;

#ifdef _WIN32
    /* Windows: Use performance counter for high-resolution timing */
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    /* Mix upper and lower 32 bits of 64-bit counter */
    seed = (unsigned int)(counter.QuadPart ^ (counter.QuadPart >> 32));

    /* Add process and thread IDs for additional entropy */
    seed ^= (unsigned int)GetCurrentProcessId();
    seed ^= (unsigned int)GetCurrentThreadId();
#else
    /* POSIX: Use monotonic clock for high-resolution timing */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    /* Combine nanoseconds and seconds */
    seed = (unsigned int)ts.tv_nsec;
    seed ^= (unsigned int)ts.tv_sec;

    /* Add process ID for additional entropy */
    seed ^= (unsigned int)getpid();
#endif

    /* Simple hash mixing for better distribution */
    /* Uses MurmurHash3-style mixing for improved avalanche properties */
    seed ^= (seed >> 16);
    seed *= 0x85ebca6b;
    seed ^= (seed >> 13);
    seed *= 0xc2b2ae35;
    seed ^= (seed >> 16);

    /* Ensure non-negative by masking sign bit */
    return (int)(seed & 0x7FFFFFFF);
}
