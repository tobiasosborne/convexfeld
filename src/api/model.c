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

/* Forward declare sparse matrix functions */
extern SparseMatrix *cxf_sparse_create(void);
extern void cxf_sparse_free(SparseMatrix *mat);

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

int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames) {
    CxfModel *model;
    int status;

    if (env == NULL || modelP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (numvars < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    *modelP = NULL;

    model = (CxfModel *)cxf_calloc(1, sizeof(CxfModel));
    if (model == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    cxf_model_init_fields(model, env, name);

    /* Determine initial capacity */
    int initial_capacity = (numvars > INITIAL_VAR_CAPACITY) ? numvars : INITIAL_VAR_CAPACITY;
    model->var_capacity = initial_capacity;

    /* Allocate initial variable arrays */
    model->obj_coeffs = (double *)cxf_malloc(
        (size_t)initial_capacity * sizeof(double));
    model->lb = (double *)cxf_malloc(
        (size_t)initial_capacity * sizeof(double));
    model->ub = (double *)cxf_malloc(
        (size_t)initial_capacity * sizeof(double));
    model->vtype = (char *)cxf_malloc(
        (size_t)initial_capacity * sizeof(char));
    model->solution = (double *)cxf_malloc(
        (size_t)initial_capacity * sizeof(double));

    if (model->obj_coeffs == NULL || model->lb == NULL ||
        model->ub == NULL || model->vtype == NULL || model->solution == NULL) {
        cxf_freemodel(model);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate constraint matrix */
    model->matrix = cxf_sparse_create();
    if (model->matrix == NULL) {
        cxf_freemodel(model);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* If numvars > 0, add initial variables using cxf_addvars */
    if (numvars > 0) {
        /* Forward declare cxf_addvars */
        extern int cxf_addvars(CxfModel *model, int numvars, int numnz,
                               const int *vbeg, const int *vind, const double *vval,
                               const double *obj, const double *lb, const double *ub,
                               const char *vtype, const char **varnames);

        status = cxf_addvars(model, numvars, 0, NULL, NULL, NULL,
                             obj, lb, ub, vtype, (const char **)varnames);
        if (status != CXF_OK) {
            cxf_freemodel(model);
            return status;
        }
    }

    *modelP = model;
    return CXF_OK;
}

void cxf_freemodel(CxfModel *model) {
    if (model == NULL) {
        return;
    }

    /* Free constraint matrix */
    cxf_sparse_free(model->matrix);

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

    /* Process pending buffer if it exists
     * For now, this is a minimal implementation that marks the model as updated.
     * Full implementation would process queued modifications from pending_buffer.
     */
    if (model->pending_buffer != NULL) {
        /* TODO: Process pending modifications when buffer implementation is ready
         * This would include:
         * - Processing variable/constraint additions
         * - Processing deletions
         * - Processing coefficient changes
         * - Rebuilding CSC matrix structure
         * - Clearing the pending buffer
         */
    }

    /* Mark model as initialized/updated */
    model->initialized = 1;

    return CXF_OK;
}
