/**
 * @file arrays.c
 * @brief Array and variable type validation functions.
 *
 * Implements validation for numeric arrays and variable types.
 * Specs: cxf_validate_array.md, cxf_validate_vartypes.md
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include <math.h>

/**
 * @brief Validate array for NaN values.
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

    /* Zero or negative count is valid */
    if (count <= 0) {
        return CXF_OK;
    }

    /* Check each element for NaN */
    for (int i = 0; i < count; i++) {
        if (isnan(array[i])) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    return CXF_OK;
}

/**
 * @brief Validate variable types and clamp binary bounds.
 *
 * Validates that all variable types are legal: C, B, I, S, N.
 * For binary variables, clamps bounds to [0, 1] and checks feasibility.
 *
 * @param model Model to validate
 * @return CXF_OK if valid, CXF_ERROR_INVALID_ARGUMENT if invalid type
 */
int cxf_validate_vartypes(CxfModel *model) {
    int i, n;
    char t;

    /* NULL model handled gracefully */
    if (model == NULL) {
        return CXF_OK;
    }

    n = model->num_vars;
    if (n <= 0) {
        return CXF_OK;
    }

    /* NULL vtype means all continuous - valid */
    if (model->vtype == NULL) {
        return CXF_OK;
    }

    /* Validate each variable type */
    for (i = 0; i < n; i++) {
        t = model->vtype[i];

        /* Check for valid type character */
        if (t != 'C' && t != 'B' && t != 'I' && t != 'S' && t != 'N') {
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Binary variables: clamp bounds to [0, 1] */
        if (t == 'B') {
            /* Clamp lower bound */
            if (model->lb != NULL) {
                if (model->lb[i] < 0.0) {
                    model->lb[i] = 0.0;
                } else if (model->lb[i] > 1.0) {
                    model->lb[i] = 1.0;
                }
            }

            /* Clamp upper bound */
            if (model->ub != NULL) {
                if (model->ub[i] < 0.0) {
                    model->ub[i] = 0.0;
                } else if (model->ub[i] > 1.0) {
                    model->ub[i] = 1.0;
                }
            }

            /* Check feasibility after clamping */
            if (model->lb != NULL && model->ub != NULL) {
                if (model->lb[i] > model->ub[i]) {
                    return CXF_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }

    return CXF_OK;
}
