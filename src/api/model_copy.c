/**
 * @file model_copy.c
 * @brief Model copy and update operations.
 *
 * Extracted from model.c to comply with 200 LOC limit.
 * Contains cxf_copymodel and cxf_updatemodel.
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

#include "../memory/memory_internal.h"

CxfModel *cxf_copymodel(CxfModel *model) {
    CxfModel *copy = NULL;
    int i;

    /* Validate source model */
    if (cxf_checkmodel(model) != CXF_OK) {
        return NULL;
    }

    /* Create new model with same environment and name (empty initially) */
    if (cxf_newmodel(model->env, &copy, model->name, 0, NULL, NULL, NULL, NULL, NULL) != CXF_OK) {
        return NULL;
    }

    /* Copy dimensions */
    copy->num_vars = model->num_vars;
    copy->num_constrs = model->num_constrs;

    /* Ensure copy has enough capacity for variables */
    if (copy->var_capacity < model->num_vars) {
        /* Reallocate arrays to match source capacity */
        int new_capacity = model->num_vars;

        cxf_free(copy->obj_coeffs);
        cxf_free(copy->lb);
        cxf_free(copy->ub);
        cxf_free(copy->vtype);
        cxf_free(copy->solution);

        copy->obj_coeffs = (double *)cxf_malloc((size_t)new_capacity * sizeof(double));
        copy->lb = (double *)cxf_malloc((size_t)new_capacity * sizeof(double));
        copy->ub = (double *)cxf_malloc((size_t)new_capacity * sizeof(double));
        copy->vtype = (char *)cxf_malloc((size_t)new_capacity * sizeof(char));
        copy->solution = (double *)cxf_malloc((size_t)new_capacity * sizeof(double));

        if (copy->obj_coeffs == NULL || copy->lb == NULL ||
            copy->ub == NULL || copy->vtype == NULL || copy->solution == NULL) {
            cxf_freemodel(copy);
            return NULL;
        }

        copy->var_capacity = new_capacity;
    }

    /* Copy variable arrays */
    for (i = 0; i < model->num_vars; i++) {
        copy->obj_coeffs[i] = model->obj_coeffs[i];
        copy->lb[i] = model->lb[i];
        copy->ub[i] = model->ub[i];
        copy->vtype[i] = model->vtype[i];
        copy->solution[i] = model->solution[i];
    }

    /* Copy status fields */
    copy->status = model->status;
    copy->obj_val = model->obj_val;
    copy->initialized = model->initialized;

    /* Note: Skipping complex pending buffer, matrix, and callback handling for now.
     * This is a simplified implementation per M8.1.10 task requirements.
     */

    return copy;
}

int cxf_updatemodel(CxfModel *model) {
    int status;

    /* Validate model */
    status = cxf_checkmodel(model);
    if (status != CXF_OK) {
        return status;
    }

    /* Not yet implemented — return error so callers don't assume success */
    return CXF_ERROR_NOT_SUPPORTED;
}
