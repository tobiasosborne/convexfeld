/**
 * @file fix_var.c
 * @brief Fix variable to a bound (M7.3.2)
 *
 * Permanently fixes a variable at a specified value by setting both
 * lower and upper bounds to that value, effectively removing it from
 * the active optimization problem.
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Fix a variable to a specified value.
 *
 * Sets both lower bound and upper bound to the specified value,
 * making the variable fixed in the problem.
 *
 * @param model Model containing the variable (must not be NULL)
 * @param var_index Variable index (must be in range [0, num_vars))
 * @param value Value to fix the variable at
 *
 * @return CXF_OK on success
 * @return CXF_ERROR_NULL_ARGUMENT if model is NULL
 * @return CXF_ERROR_INVALID_ARGUMENT if var_index is out of range
 */
int cxf_fix_variable(CxfModel *model, int var_index, double value) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (var_index < 0 || var_index >= model->num_vars) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Fix variable by setting both bounds to the same value */
    model->lb[var_index] = value;
    model->ub[var_index] = value;

    return CXF_OK;
}
