/**
 * @file api_stub.c
 * @brief Stub optimization and attribute functions for tracer bullet.
 *
 * Minimal implementation of cxf_optimize and attribute getters.
 * The optimizer solves trivial unconstrained LPs by setting each
 * variable to its bound that minimizes the objective.
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/**
 * @brief Optimize the model (stub - trivial unconstrained solver).
 *
 * This stub solves unconstrained LPs by setting each variable to
 * its lower or upper bound based on the objective coefficient sign.
 *
 * For minimization:
 *   - If obj_coeff >= 0, set x = lb (lower is better)
 *   - If obj_coeff < 0, set x = ub (higher is better, but coeff negative)
 *
 * @param model Model to optimize
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize(CxfModel *model) {
    int i;
    double objval;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Trivial solver: set each variable to optimal bound */
    objval = 0.0;
    for (i = 0; i < model->num_vars; i++) {
        double coeff = model->obj_coeffs[i];
        double lb = model->lb[i];
        double ub = model->ub[i];
        double val;

        /* For minimization: positive coeff -> use lb, negative -> use ub */
        if (coeff >= 0.0) {
            val = lb;
        } else {
            val = ub;
        }

        /* Check for unbounded */
        if (val <= -CXF_INFINITY || val >= CXF_INFINITY) {
            model->status = CXF_UNBOUNDED;
            return CXF_OK;
        }

        model->solution[i] = val;
        objval += coeff * val;
    }

    model->obj_val = objval;
    model->status = CXF_OPTIMAL;

    return CXF_OK;
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
