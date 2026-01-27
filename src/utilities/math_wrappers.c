/**
 * @file math_wrappers.c
 * @brief Safe wrappers for standard math functions (M7.3.3)
 *
 * Provides defensive wrappers for math.h functions with explicit handling
 * of special cases (NaN, infinity, domain errors). Ensures consistent
 * behavior across platforms.
 */

#define _POSIX_C_SOURCE 199309L

#include <math.h>
#include <stddef.h>

/**
 * @brief Safe wrapper for base-10 logarithm.
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - Negative input -> NaN (domain error)
 * - Zero input -> -Infinity
 * - +Infinity input -> +Infinity
 * - Positive finite -> log10(value)
 *
 * @param value Input value
 * @return Base-10 logarithm or special value
 */
double cxf_log10_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Domain error: negative values */
    if (value < 0.0) {
        return NAN;
    }

    /* Mathematical limit: log10(0) = -Inf */
    if (value == 0.0) {
        return -INFINITY;
    }

    /* Infinity propagation */
    if (isinf(value)) {
        return value;  /* +Inf -> +Inf */
    }

    /* Normal case: positive finite value */
    return log10(value);
}

/**
 * @brief Safe wrapper for square root.
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - Negative input -> NaN (domain error)
 * - Zero input -> 0.0
 * - +Infinity input -> +Infinity
 * - Positive finite -> sqrt(value)
 *
 * @param value Input value
 * @return Square root or special value
 */
double cxf_sqrt_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Domain error: negative values */
    if (value < 0.0) {
        return NAN;
    }

    /* Infinity propagation */
    if (isinf(value)) {
        return value;  /* +Inf -> +Inf */
    }

    /* Normal case: non-negative finite value */
    return sqrt(value);
}

/**
 * @brief Safe wrapper for absolute value.
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - ±Infinity input -> +Infinity
 * - Finite -> fabs(value)
 *
 * @param value Input value
 * @return Absolute value or special value
 */
double cxf_fabs_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Infinity becomes positive infinity */
    if (isinf(value)) {
        return INFINITY;  /* Both +Inf and -Inf -> +Inf */
    }

    /* Normal case: finite value */
    return fabs(value);
}

/**
 * @brief Safe wrapper for floor function (round down).
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - ±Infinity input -> ±Infinity
 * - Finite -> floor(value)
 *
 * @param value Input value
 * @return Floor of value or special value
 */
double cxf_floor_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Infinity propagation */
    if (isinf(value)) {
        return value;
    }

    /* Normal case: finite value */
    return floor(value);
}

/**
 * @brief Safe wrapper for ceiling function (round up).
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - ±Infinity input -> ±Infinity
 * - Finite -> ceil(value)
 *
 * @param value Input value
 * @return Ceiling of value or special value
 */
double cxf_ceil_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Infinity propagation */
    if (isinf(value)) {
        return value;
    }

    /* Normal case: finite value */
    return ceil(value);
}

/**
 * @brief Safe wrapper for power function.
 *
 * Handles special cases following IEEE 754:
 * - Any NaN input -> NaN output
 * - pow(0, negative) -> +Infinity
 * - pow(negative, non-integer) -> NaN
 * - Other special cases delegated to standard pow()
 *
 * @param base Base value
 * @param exponent Exponent value
 * @return base^exponent or special value
 */
double cxf_pow_wrapper(double base, double exponent) {
    /* NaN propagation */
    if (isnan(base) || isnan(exponent)) {
        return NAN;
    }

    /* Special case: 0^(negative) = +Inf */
    if (base == 0.0 && exponent < 0.0) {
        return INFINITY;
    }

    /* Special case: negative^(non-integer) = NaN */
    if (base < 0.0 && !isnan(exponent) && floor(exponent) != exponent) {
        return NAN;
    }

    /* Normal case: delegate to standard pow */
    return pow(base, exponent);
}

/**
 * @brief Safe wrapper for exponential function.
 *
 * Handles special cases explicitly:
 * - NaN input -> NaN output
 * - +Infinity input -> +Infinity
 * - -Infinity input -> 0.0
 * - Finite -> exp(value)
 *
 * @param value Input value
 * @return e^value or special value
 */
double cxf_exp_wrapper(double value) {
    /* NaN propagation */
    if (isnan(value)) {
        return value;
    }

    /* Infinity cases */
    if (isinf(value)) {
        if (value > 0.0) {
            return INFINITY;  /* exp(+Inf) = +Inf */
        } else {
            return 0.0;  /* exp(-Inf) = 0 */
        }
    }

    /* Normal case: finite value */
    return exp(value);
}
