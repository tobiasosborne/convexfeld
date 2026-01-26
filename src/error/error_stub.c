/**
 * @file error_stub.c
 * @brief Stub error handling functions for tracer bullet.
 *
 * Remaining stub implementations for error handling.
 * Implementations moved to dedicated files:
 * - cxf_error, cxf_geterrormsg, cxf_errorlog: core.c (M3.1.2)
 * - cxf_check_nan, cxf_check_nan_or_inf: nan_check.c (M3.1.3)
 * - cxf_checkenv: env_check.c (M3.1.4)
 */

#include "convexfeld/cxf_types.h"

/**
 * @brief Check if pivot element is numerically acceptable.
 *
 * @param pivot_elem Pivot element value
 * @param tolerance Minimum acceptable magnitude
 * @return 1 if valid, 0 if invalid (too small, NaN, etc.)
 */
int cxf_pivot_check(double pivot_elem, double tolerance) {
    /* Check for NaN */
    if (pivot_elem != pivot_elem) {
        return 0;
    }
    /* Check magnitude */
    double abs_val = (pivot_elem < 0) ? -pivot_elem : pivot_elem;
    if (abs_val < tolerance) {
        return 0;
    }
    return 1;
}
