/**
 * @file pivot_check.c
 * @brief Pivot validation functions (M3.1.7)
 *
 * Functions for validating pivot operations:
 * - cxf_validate_pivot_element: validate pivot element magnitude
 * - cxf_special_check: validate variable for special pivot handling
 *
 * Specs:
 * - docs/specs/functions/ratio_test/cxf_pivot_check.md
 * - docs/specs/functions/statistics/cxf_special_check.md
 */

#include "convexfeld/cxf_types.h"
#include <math.h>

/* Flag bits for variable flags */
#define VARFLAG_UPPER_FINITE   0x04
#define VARFLAG_HAS_QUADRATIC  0x08
#define VARFLAG_RESERVED_MASK  0xFFFFFFB0

/* Threshold values */
#define NEG_INFINITY_THRESHOLD (-1e99)
#define POS_INFINITY_THRESHOLD (1e99)

/**
 * @brief Check if a pivot element is numerically acceptable.
 *
 * Validates that the pivot element is:
 * - Not NaN
 * - Not too small in magnitude
 *
 * @param pivot_elem The pivot element value
 * @param tolerance Minimum acceptable magnitude
 * @return 1 if valid, 0 if invalid
 */
int cxf_validate_pivot_element(double pivot_elem, double tolerance) {
    /* Check for NaN */
    if (isnan(pivot_elem)) {
        return 0;
    }

    /* Check magnitude */
    if (fabs(pivot_elem) < tolerance) {
        return 0;
    }

    return 1;
}

/**
 * @brief Check if a variable qualifies for special pivot handling.
 *
 * Validates that the variable meets requirements for optimized
 * pivot operations:
 * - Finite lower bound
 * - No reserved flag bits set
 * - Valid upper bound if flagged as finite
 * - Non-negative quadratic terms if present
 *
 * This is a simplified implementation for LP-only (no quadratic).
 *
 * @param lb Lower bound of the variable
 * @param ub Upper bound of the variable
 * @param flags Variable flags
 * @param work_accum Optional work accumulator (may be NULL)
 * @return 1 if qualifies for special pivot, 0 otherwise
 */
int cxf_special_check(double lb, double ub, uint32_t flags,
                      double *work_accum) {
    (void)ub;  /* Currently unused in LP-only implementation */
    (void)work_accum;  /* Work tracking not implemented yet */

    /* Check 1: Finite lower bound */
    if (lb < NEG_INFINITY_THRESHOLD) {
        return 0;
    }

    /* Check 2: Reserved flags must not be set */
    if ((flags & VARFLAG_RESERVED_MASK) != 0) {
        return 0;
    }

    /* Check 3: If upper bound flagged as finite, validate */
    if ((flags & VARFLAG_UPPER_FINITE) != 0) {
        if (lb > POS_INFINITY_THRESHOLD) {
            return 0;
        }
    }

    /* Check 4: Quadratic handling (not implemented for LP-only)
     * If quadratic flag is set, reject for now */
    if ((flags & VARFLAG_HAS_QUADRATIC) != 0) {
        return 0;  /* Quadratic not supported yet */
    }

    return 1;
}
