/**
 * @file system.c
 * @brief System information functions for logging (M3.2.4)
 *
 * Provides platform-independent system queries:
 * - cxf_detect_logical_processors: Internal detection helper
 * - cxf_get_logical_processors: V2 accessor returning cached env value
 */

/* For sysconf on POSIX systems */
#define _POSIX_C_SOURCE 199309L

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Detect the number of logical processors available (internal helper).
 *
 * Called once during environment initialization. The result is cached
 * in env->logical_processors.
 *
 * @return Number of logical processors (minimum 1)
 */
int cxf_detect_logical_processors(void) {
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

/**
 * @brief Return cached logical processor count from environment.
 *
 * Per V2 threading_sync.md: pure accessor, returns immutable value
 * set during environment initialization.
 *
 * @param env Environment containing hardware detection results
 * @return Number of logical processors
 */
int cxf_get_logical_processors(CxfEnv *env) {
    if (env == NULL) {
        return 1;
    }
    return env->logical_processors;
}
