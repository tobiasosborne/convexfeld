/**
 * @file attrs_api.c
 * @brief Attribute API implementation (M8.1.15)
 *
 * Implements attribute getters for integer and double model attributes.
 * Extracted from api_stub.c with expanded attribute support.
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Forward declaration - implemented in src/analysis/coef_stats.c */
int cxf_compute_coef_stats(CxfModel *model,
                           double *obj_min, double *obj_max,
                           double *bounds_min, double *bounds_max,
                           double *matrix_min, double *matrix_max);

/**
 * @brief Get an integer attribute value.
 *
 * Supported attributes:
 *   - "Status": Optimization status (model->status)
 *   - "NumVars": Number of variables
 *   - "NumConstrs": Number of constraints
 *   - "ModelSense": 1 for minimize, -1 for maximize (default 1)
 *   - "IsMIP": 0 (LP only for now)
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

    if (strcmp(attrname, "ModelSense") == 0) {
        /* Default to minimize (1). In the future, read from model field. */
        *valueP = 1;
        return CXF_OK;
    }

    if (strcmp(attrname, "IsMIP") == 0) {
        /* LP only for now */
        *valueP = 0;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Get a double attribute value.
 *
 * Supported attributes:
 *   - "ObjVal": Objective value
 *   - "Runtime": model->update_time
 *   - "ObjBound": Same as ObjVal for LP
 *   - "ObjBoundC": Same as ObjVal for LP
 *   - "MaxCoeff": max |a_ij| from constraint matrix
 *   - "MinCoeff": min |a_ij| (nonzero) from constraint matrix
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

    if (strcmp(attrname, "Runtime") == 0) {
        *valueP = model->update_time;
        return CXF_OK;
    }

    if (strcmp(attrname, "ObjBound") == 0) {
        /* For LP, ObjBound = ObjVal */
        *valueP = model->obj_val;
        return CXF_OK;
    }

    if (strcmp(attrname, "ObjBoundC") == 0) {
        /* For LP, ObjBoundC = ObjVal */
        *valueP = model->obj_val;
        return CXF_OK;
    }

    if (strcmp(attrname, "MaxCoeff") == 0) {
        double matrix_min, matrix_max;
        int rc = cxf_compute_coef_stats(model, NULL, NULL,
                                        NULL, NULL,
                                        &matrix_min, &matrix_max);
        if (rc != CXF_OK) return rc;
        *valueP = matrix_max;
        return CXF_OK;
    }

    if (strcmp(attrname, "MinCoeff") == 0) {
        double matrix_min, matrix_max;
        int rc = cxf_compute_coef_stats(model, NULL, NULL,
                                        NULL, NULL,
                                        &matrix_min, &matrix_max);
        if (rc != CXF_OK) return rc;
        *valueP = matrix_min;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}
