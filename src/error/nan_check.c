/**
 * @file nan_check.c
 * @brief NaN and Infinity detection functions (M3.1.3)
 *
 * Numeric validation functions for detecting invalid floating-point values.
 * Uses isfinite() and NaN self-comparison for portable detection.
 */

#include <math.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Check array for NaN values.
 *
 * Scans array for Not-a-Number values. Uses the property that
 * NaN != NaN (IEEE 754) for portable detection.
 *
 * Does NOT detect infinity - use cxf_check_nan_or_inf for that.
 *
 * @param arr Array to check
 * @param n Number of elements
 * @return 0 if clean (no NaN), 1 if NaN found, -1 on error (NULL array)
 */
int cxf_check_nan(const double *arr, int n) {
    if (arr == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (arr[i] != arr[i]) {  /* NaN != NaN (IEEE 754) */
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Check array for NaN or Infinity values.
 *
 * Scans array for any non-finite values (NaN, +Infinity, -Infinity).
 * Uses isfinite() from math.h for comprehensive detection.
 *
 * @param arr Array to check
 * @param n Number of elements
 * @return 0 if all finite, 1 if NaN/Inf found, -1 on error (NULL array)
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
