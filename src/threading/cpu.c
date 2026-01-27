/**
 * @file cpu.c
 * @brief CPU detection and information
 */

#define _POSIX_C_SOURCE 199309L
#include "convexfeld/cxf_types.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#endif

/* Defined in logging/system.c */
int cxf_get_logical_processors(void);

/**
 * @brief Get the number of physical CPU cores
 *
 * Attempts to detect actual physical cores (excluding hyperthreading).
 * Falls back to logical processor count if detection fails.
 *
 * @return Number of physical cores (always >= 1)
 */
int cxf_get_physical_cores(void) {
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

    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(length);
    if (buffer == NULL) {
        goto fallback;
    }

    if (!GetLogicalProcessorInformation(buffer, &length)) {
        free(buffer);
        goto fallback;
    }

    /* Count processor cores */
    DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationProcessorCore) {
            physical_cores++;
        }
    }

    free(buffer);

    if (physical_cores > 0) {
        return physical_cores;
    }

fallback:
    return cxf_get_logical_processors();

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
    return cxf_get_logical_processors();
#endif
}
