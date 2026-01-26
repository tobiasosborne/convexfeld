/**
 * @file validation_stub.c
 * @brief Stub implementations for validation functions.
 *
 * These stubs allow TDD tests to link. Real implementations will
 * replace these functions in separate files.
 */

#include "convexfeld/cxf_types.h"
#include <math.h>

/**
 * @brief Validate array for NaN values (stub).
 *
 * Checks that array does not contain NaN values.
 * Infinity is allowed per spec (valid for bounds).
 *
 * @param env   Environment pointer (unused in validation)
 * @param count Number of elements in array
 * @param array Array to validate (NULL is valid)
 * @return CXF_OK if valid, CXF_ERROR_INVALID_ARGUMENT if NaN found
 */
int cxf_validate_array(CxfEnv *env, int count, const double *array) {
    (void)env;  /* Unused */

    /* NULL array is valid (indicates defaults) */
    if (array == NULL) {
        return CXF_OK;
    }

    /* Zero or negative count is valid (empty/invalid treated as valid) */
    if (count <= 0) {
        return CXF_OK;
    }

    /* Check each element for NaN */
    for (int i = 0; i < count; i++) {
        if (isnan(array[i])) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        /* Note: Infinity is allowed per spec */
    }

    return CXF_OK;
}

/**
 * @brief Validate variable types in model (stub).
 *
 * Validates that all variable types are legal: C, B, I, S, N.
 * For binary variables, clamps bounds to [0, 1].
 *
 * @param model Model to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_validate_vartypes(CxfModel *model) {
    /* NULL model handled gracefully */
    if (model == NULL) {
        return CXF_OK;  /* Or CXF_ERROR_NULL_ARGUMENT if stricter */
    }

    /* Stub: full implementation requires MatrixData access */
    return CXF_OK;
}
