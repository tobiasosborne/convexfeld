/**
 * @file cxf_utilities.h
 * @brief Utility functions - math wrappers and helper functions.
 *
 * This header provides utility functions used throughout ConvexFeld:
 * - Safe wrappers for standard math functions (log10, sqrt, floor, etc.)
 * - Model inspection helpers (multi-objective detection)
 * - Constraint analysis utilities
 */

#ifndef CXF_UTILITIES_H
#define CXF_UTILITIES_H

#include "cxf_types.h"

/*******************************************************************************
 * Math Function Wrappers
 ******************************************************************************/

/**
 * @brief Safe wrapper for base-10 logarithm.
 *
 * Handles special cases (NaN, negative, zero, infinity) explicitly.
 *
 * @param value Input value
 * @return log₁₀(value) for positive finite values, special value otherwise
 */
double cxf_log10_wrapper(double value);

/**
 * @brief Safe wrapper for square root.
 *
 * Handles special cases (NaN, negative, infinity) explicitly.
 *
 * @param value Input value
 * @return √value for non-negative finite values, special value otherwise
 */
double cxf_sqrt_wrapper(double value);

/**
 * @brief Safe wrapper for absolute value.
 *
 * Handles special cases (NaN, infinity) explicitly.
 *
 * @param value Input value
 * @return |value| for finite values, special value otherwise
 */
double cxf_fabs_wrapper(double value);

/**
 * @brief Safe wrapper for floor function (round down).
 *
 * Handles special cases (NaN, infinity) explicitly.
 *
 * @param value Input value
 * @return ⌊value⌋ for finite values, special value otherwise
 */
double cxf_floor_wrapper(double value);

/**
 * @brief Safe wrapper for ceiling function (round up).
 *
 * Handles special cases (NaN, infinity) explicitly.
 *
 * @param value Input value
 * @return ⌈value⌉ for finite values, special value otherwise
 */
double cxf_ceil_wrapper(double value);

/**
 * @brief Safe wrapper for power function.
 *
 * Handles special cases following IEEE 754 semantics.
 *
 * @param base Base value
 * @param exponent Exponent value
 * @return base^exponent or special value
 */
double cxf_pow_wrapper(double base, double exponent);

/**
 * @brief Safe wrapper for exponential function.
 *
 * Handles special cases (NaN, ±infinity) explicitly.
 *
 * @param value Input value
 * @return e^value for finite values, special value otherwise
 */
double cxf_exp_wrapper(double value);

/*******************************************************************************
 * Model Inspection Helpers
 ******************************************************************************/

/**
 * @brief Check if model has multiple objectives.
 *
 * Determines whether a model uses multi-objective optimization (NumObj > 1).
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has multiple objectives, 0 otherwise
 */
int cxf_is_multi_objective(CxfModel *model);

#endif /* CXF_UTILITIES_H */
