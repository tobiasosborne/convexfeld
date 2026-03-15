/**
 * @file cpu.c
 * @brief CPU detection and information
 *
 * - cxf_detect_physical_cores: Internal detection helper
 * - cxf_get_physical_cores: V2 accessor returning min(logical, physical)
 */

#define _POSIX_C_SOURCE 199309L
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "../memory/memory_internal.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#endif

/* Internal detection helper from logging/system.c */
int cxf_detect_logical_processors(void);

/**
 * @brief Detect the number of physical CPU cores (internal helper).
 *
 * Called once during environment initialization. The result is cached
 * in env->physical_cores.
 *
 * @return Number of physical cores (always >= 1)
 */
int cxf_detect_physical_cores(void) {
#ifdef _WIN32
    /* Windows: Use GetLogicalProcessorInformation */
    DWORD length = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    int physical_cores = 0;

    /* Get required buffer size */
    GetLogicalProcessorInformation(NULL, &length);
    if (length == 0) {
        goto fallback;
    }

    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)cxf_malloc(length);
    if (buffer == NULL) {
        goto fallback;
    }

    if (!GetLogicalProcessorInformation(buffer, &length)) {
        cxf_free(buffer);
        goto fallback;
    }

    /* Count processor cores */
    DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationProcessorCore) {
            physical_cores++;
        }
    }

    cxf_free(buffer);

    if (physical_cores > 0) {
        return physical_cores;
    }

fallback:
    return cxf_detect_logical_processors();

#else
    /* Linux: Try reading from /sys/devices/system/cpu/present */
    FILE *fp = fopen("/sys/devices/system/cpu/present", "r");
    if (fp != NULL) {
        int first = 0, last = 0;
        if (fscanf(fp, "%d-%d", &first, &last) == 2) {
            fclose(fp);
            int cores = last - first + 1;
            if (cores > 0) {
                return cores;
            }
        }
        fclose(fp);
    }

    /* Try sysconf with _SC_NPROCESSORS_CONF */
    long conf_cores = sysconf(_SC_NPROCESSORS_CONF);
    if (conf_cores > 0) {
        return (int)conf_cores;
    }

    /* Fallback to logical processors */
    return cxf_detect_logical_processors();
#endif
}

/**
 * @brief Return min(logical, physical) from cached environment values.
 *
 * Per V2 threading_sync.md: returns a conservative estimate by taking
 * the minimum of logical processor count and physical core count.
 *
 * @param env Environment containing hardware detection results
 * @return Effective physical core count
 */
int cxf_get_physical_cores(CxfEnv *env) {
    if (env == NULL) {
        return 1;
    }
    if (env->logical_processors < env->physical_cores) {
        return env->logical_processors;
    }
    return env->physical_cores;
}
