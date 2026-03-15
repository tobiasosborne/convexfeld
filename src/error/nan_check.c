/**
 * @file nan_check.c
 * @brief NaN and Infinity detection functions (M3.1.3)
 *
 * V2 spec-compliant single-value functions per input_validation.md:
 *   cxf_check_nan       — 1 if NaN, 0 otherwise
 *   cxf_check_is_finite — 1 if finite, 0 otherwise
 *   cxf_check_nan_or_inf — alias for cxf_check_is_finite
 *
 * Legacy array-based helpers retained for backward compatibility:
 *   cxf_check_nan_array       — scan array for NaN
 *   cxf_check_is_finite_array — scan array for non-finite values
 */

#include <math.h>
#include "convexfeld/cxf_types.h"

/*============================================================================
 * V2 Spec-Compliant Single-Value Functions (input_validation.md)
 *===========================================================================*/

/**
 * @brief Check if a double value is NaN (IEEE 754).
 *
 * Pure function. Uses NaN != NaN property.
 *
 * @param val Value to test
 * @return 1 if NaN, 0 otherwise
 */
int cxf_check_nan(double val) {
    return val != val;  /* NaN != NaN (IEEE 754) */
}

/**
 * @brief Check if a double value is finite (not NaN, not Inf).
 *
 * Pure function. Uses isfinite() from C99 math.h.
 *
 * @param val Value to test
 * @return 1 if finite, 0 if NaN or infinity
 */
int cxf_check_is_finite(double val) {
    return isfinite(val) != 0;
}

/**
 * @brief Alias for cxf_check_is_finite.
 *
 * Per V2 spec (input_validation.md): despite the name suggesting it
 * returns true for NaN/Inf, it actually returns true for finite values.
 * Historical misnomer preserved for backward compatibility.
 *
 * @param val Value to test
 * @return 1 if finite, 0 if NaN or infinity
 */
int cxf_check_nan_or_inf(double val) {
    return cxf_check_is_finite(val);
}

/*============================================================================
 * Legacy Array-Based Helpers (backward compatibility)
 *===========================================================================*/

/**
 * @brief Scan array for NaN values.
 *
 * @param arr Array to check
 * @param n   Number of elements
 * @return 0 if clean, 1 if NaN found, -1 on NULL
 */
int cxf_check_nan_array(const double *arr, int n) {
    if (arr == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (arr[i] != arr[i]) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Scan array for non-finite values (NaN or Inf).
 *
 * @param arr Array to check
 * @param n   Number of elements
 * @return 0 if all finite, 1 if non-finite found, -1 on NULL
 */
int cxf_check_is_finite_array(const double *arr, int n) {
    if (arr == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (!isfinite(arr[i])) {
            return 1;
        }
    }
    return 0;
}
