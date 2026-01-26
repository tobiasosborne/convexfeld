/**
 * @file model_stub.c
 * @brief Stub model functions for tracer bullet.
 *
 * Minimal implementation of model creation and variable addition.
 * Full implementation with constraint support comes later.
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Forward declare memory functions */
extern void *cxf_calloc(size_t count, size_t size);
extern void *cxf_malloc(size_t size);
extern void cxf_free(void *ptr);

/* Initial capacity for variable arrays */
#define INITIAL_VAR_CAPACITY 16

/**
 * @brief Create a new empty model (stub).
 *
 * @param env Parent environment
 * @param modelP Output pointer to created model
 * @param name Model name (may be NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name) {
    CxfModel *model;

    if (env == NULL || modelP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *modelP = NULL;

    model = (CxfModel *)cxf_calloc(1, sizeof(CxfModel));
    if (model == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Initialize fields */
    model->magic = CXF_MODEL_MAGIC;
    model->env = env;
    model->num_vars = 0;
    model->num_constrs = 0;
    model->status = CXF_OK;
    model->obj_val = 0.0;
    model->initialized = 0;
    model->modification_blocked = 0;

    /* Copy name if provided */
    if (name != NULL) {
        strncpy(model->name, name, CXF_MAX_NAME_LEN);
        model->name[CXF_MAX_NAME_LEN] = '\0';
    } else {
        model->name[0] = '\0';
    }

    /* Allocate initial variable arrays */
    model->obj_coeffs = (double *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(double));
    model->lb = (double *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(double));
    model->ub = (double *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(double));
    model->solution = (double *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(double));

    if (model->obj_coeffs == NULL || model->lb == NULL ||
        model->ub == NULL || model->solution == NULL) {
        cxf_freemodel(model);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    *modelP = model;
    return CXF_OK;
}

/**
 * @brief Free a model and all associated resources (stub).
 *
 * @param model Model to free (may be NULL)
 */
void cxf_freemodel(CxfModel *model) {
    if (model == NULL) {
        return;
    }

    /* Free variable arrays */
    cxf_free(model->obj_coeffs);
    cxf_free(model->lb);
    cxf_free(model->ub);
    cxf_free(model->vtype);
    cxf_free(model->solution);
    cxf_free(model->pi);

    /* Mark as invalid before freeing */
    model->magic = 0;
    model->env = NULL;

    cxf_free(model);
}

/**
 * @brief Add a single variable to the model (stub).
 *
 * This stub only supports adding up to INITIAL_VAR_CAPACITY variables.
 * Full implementation handles dynamic resizing.
 *
 * @param model Target model
 * @param lb Lower bound
 * @param ub Upper bound
 * @param obj Objective coefficient
 * @param vtype Variable type (ignored in stub - all continuous)
 * @param name Variable name (ignored in stub)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addvar(CxfModel *model, double lb, double ub, double obj,
               char vtype, const char *name) {
    int idx;

    (void)vtype;  /* Unused in stub */
    (void)name;   /* Unused in stub */

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (model->num_vars >= INITIAL_VAR_CAPACITY) {
        return CXF_ERROR_OUT_OF_MEMORY;  /* Would need realloc */
    }

    idx = model->num_vars;
    model->obj_coeffs[idx] = obj;
    model->lb[idx] = lb;
    model->ub[idx] = ub;
    model->solution[idx] = 0.0;
    model->num_vars++;

    return CXF_OK;
}
