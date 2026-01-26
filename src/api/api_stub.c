/**
 * @file api_stub.c
 * @brief Stub optimization and attribute functions for tracer bullet.
 *
 * Minimal implementation of cxf_optimize and attribute getters.
 * Delegates actual solving to cxf_solve_lp() in simplex module.
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Forward declaration - implemented in src/simplex/solve_lp_stub.c */
int cxf_solve_lp(CxfModel *model);

/**
 * @brief Optimize the model.
 *
 * Public API entry point for optimization. Delegates to the
 * appropriate solver based on model type. Currently only LP
 * via simplex is supported.
 *
 * @param model Model to optimize
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize(CxfModel *model) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Delegate to simplex solver */
    return cxf_solve_lp(model);
}

/**
 * @brief Get an integer attribute value (stub).
 *
 * Supported attributes:
 *   - "Status": Optimization status (CxfStatus)
 *   - "NumVars": Number of variables
 *   - "NumConstrs": Number of constraints
 *
 * @param model Model to query
 * @param attrname Attribute name
 * @param valueP Output value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getintattr(CxfModel *model, const char *attrname, int *valueP) {
    if (model == NULL || attrname == NULL || valueP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (strcmp(attrname, "Status") == 0) {
        *valueP = model->status;
        return CXF_OK;
    }

    if (strcmp(attrname, "NumVars") == 0) {
        *valueP = model->num_vars;
        return CXF_OK;
    }

    if (strcmp(attrname, "NumConstrs") == 0) {
        *valueP = model->num_constrs;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Get a double attribute value (stub).
 *
 * Supported attributes:
 *   - "ObjVal": Objective value
 *
 * @param model Model to query
 * @param attrname Attribute name
 * @param valueP Output value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getdblattr(CxfModel *model, const char *attrname, double *valueP) {
    if (model == NULL || attrname == NULL || valueP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (strcmp(attrname, "ObjVal") == 0) {
        *valueP = model->obj_val;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}
