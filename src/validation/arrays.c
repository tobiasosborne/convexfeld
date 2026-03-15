/**
 * @file arrays.c
 * @brief Array and variable type validation functions.
 *
 * Implements validation for numeric arrays and variable types.
 * Spec: data_validation.md (cxf_validate_array, cxf_validate_vartypes)
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_error.h"
#include <math.h>

/**
 * @brief Validate array for NaN values.
 *
 * Checks that array does not contain NaN values.
 * Infinity is allowed per spec (valid for bounds).
 *
 * @param env   Environment pointer (for error reporting, may be NULL)
 * @param count Number of elements in array
 * @param array Array to validate (NULL is valid)
 * @return CXF_OK if valid, CXF_ERROR_INVALID_ARGUMENT if NaN found
 */
int cxf_validate_array(CxfEnv *env, int count, const double *array) {
    /* NULL array is valid (vacuously valid) */
    if (array == NULL) {
        return CXF_OK;
    }

    /* Zero or negative count is valid (vacuously valid) */
    if (count <= 0) {
        return CXF_OK;
    }

    /* Check each element for NaN (early-exit on first) */
    for (int i = 0; i < count; i++) {
        if (isnan(array[i])) {
            if (env != NULL) {
                cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 0,
                              "NaN at index %d", i);
            }
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    return CXF_OK;
}

/**
 * @brief Validate variable type array per data_validation.md V2 spec.
 *
 * Validates that every element is a recognized LP variable type code
 * (C, B, I, S, N). Case-insensitive: lowercase letters are normalized
 * to uppercase before comparison. Pure validation -- no side effects.
 *
 * @param env      Environment pointer (for error logging, may be NULL)
 * @param count    Number of type codes in the array
 * @param vartypes Array of variable type characters (NULL is valid)
 * @return CXF_OK if valid, CXF_ERROR_INVALID_ARGUMENT if invalid code
 */
int cxf_validate_vartypes(CxfEnv *env, int count, const char *vartypes) {
    /* NULL vartypes is vacuously valid */
    if (vartypes == NULL) {
        return CXF_OK;
    }

    /* Zero or negative count is vacuously valid */
    if (count <= 0) {
        return CXF_OK;
    }

    /* Validate each variable type (early-exit on first invalid) */
    for (int i = 0; i < count; i++) {
        char orig = vartypes[i];
        /* Case normalization: lowercase -> uppercase */
        char t = orig;
        if (t >= 'a' && t <= 'z') {
            t = (char)(t - ('a' - 'A'));
        }

        if (t != 'C' && t != 'B' && t != 'I' && t != 'S' && t != 'N') {
            if (env != NULL) {
                cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 0,
                              "Invalid variable type '%c' at index %d",
                              orig, i);
            }
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    return CXF_OK;
}
