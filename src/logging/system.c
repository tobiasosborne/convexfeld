/**
 * @file system.c
 * @brief System information functions for logging (M3.2.4)
 *
 * Provides platform-independent system queries:
 * - cxf_get_logical_processors: Get CPU count for thread validation
 */

/* For sysconf on POSIX systems */
#define _POSIX_C_SOURCE 199309L

#include "convexfeld/cxf_types.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Get the number of logical processors available.
 *
 * Detects the total number of logical processors (including hyperthreads).
 * Used for thread count validation and as a fallback when physical
 * core detection is unavailable.
 *
 * @return Number of logical processors (minimum 1)
 */
int cxf_get_logical_processors(void) {
    int count = 0;

#ifdef _WIN32
    /* Windows: Use GetSystemInfo */
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    count = (int)sys_info.dwNumberOfProcessors;
#else
    /* POSIX (Linux, macOS, etc.): Use sysconf */
    long result = sysconf(_SC_NPROCESSORS_ONLN);
    if (result > 0) {
        count = (int)result;
    }
#endif

    /* Ensure minimum return value of 1 per spec */
    return (count > 0) ? count : 1;
}
