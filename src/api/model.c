/**
 * @file model.c
 * @brief Full CxfModel structure implementation (M8.1.8)
 *
 * Implements model lifecycle and accessor functions per spec.
 * Variable manipulation functions are in model_stub.c until M8.1.11.
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
 * @brief Internal helper to initialize model fields.
 *
 * @param model Model to initialize
 * @param env Parent environment
 * @param name Model name (may be NULL)
 */
static void cxf_model_init_fields(CxfModel *model, CxfEnv *env,
                                  const char *name) {
    model->magic = CXF_MODEL_MAGIC;
    model->env = env;

    /* Copy name if provided */
    if (name != NULL) {
        strncpy(model->name, name, CXF_MAX_NAME_LEN);
        model->name[CXF_MAX_NAME_LEN] = '\0';
    } else {
        model->name[0] = '\0';
    }

    /* Dimensions */
    model->num_vars = 0;
    model->num_constrs = 0;
    model->var_capacity = INITIAL_VAR_CAPACITY;

    /* Status */
    model->status = CXF_OK;
    model->obj_val = 0.0;
    model->initialized = 0;
    model->modification_blocked = 0;

    /* Extended fields */
    model->fingerprint = 0;
    model->update_time = 0.0;
    model->pending_buffer = NULL;
    model->solution_data = NULL;
    model->sos_data = NULL;
    model->gen_constr_data = NULL;

    /* Self-reference */
    model->primary_model = model;  /* Points to self by default */
    model->self_ptr = NULL;        /* Set during optimization */

    /* Bookkeeping */
    model->callback_count = 0;
    model->solve_mode = 0;
    model->env_flag = 0;
}

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

    cxf_model_init_fields(model, env, name);

    /* Allocate initial variable arrays */
    model->obj_coeffs = (double *)cxf_malloc(
        (size_t)INITIAL_VAR_CAPACITY * sizeof(double));
    model->lb = (double *)cxf_malloc(
        (size_t)INITIAL_VAR_CAPACITY * sizeof(double));
    model->ub = (double *)cxf_malloc(
        (size_t)INITIAL_VAR_CAPACITY * sizeof(double));
    model->vtype = (char *)cxf_malloc(
        (size_t)INITIAL_VAR_CAPACITY * sizeof(char));
    model->solution = (double *)cxf_malloc(
        (size_t)INITIAL_VAR_CAPACITY * sizeof(double));

    if (model->obj_coeffs == NULL || model->lb == NULL ||
        model->ub == NULL || model->vtype == NULL || model->solution == NULL) {
        cxf_freemodel(model);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    *modelP = model;
    return CXF_OK;
}

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

    /* Free optional structures */
    cxf_free(model->pending_buffer);
    cxf_free(model->solution_data);
    cxf_free(model->sos_data);
    cxf_free(model->gen_constr_data);

    /* Mark as invalid before freeing */
    model->magic = 0;
    model->env = NULL;
    model->primary_model = NULL;
    model->self_ptr = NULL;

    cxf_free(model);
}

int cxf_checkmodel(CxfModel *model) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (model->magic != CXF_MODEL_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    return CXF_OK;
}

int cxf_model_is_blocked(CxfModel *model) {
    if (model == NULL || model->magic != CXF_MODEL_MAGIC) {
        return -1;
    }
    return model->modification_blocked;
}
