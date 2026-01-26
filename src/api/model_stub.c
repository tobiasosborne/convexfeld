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
    model->vtype = (char *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(char));
    model->solution = (double *)cxf_malloc(INITIAL_VAR_CAPACITY * sizeof(double));

    if (model->obj_coeffs == NULL || model->lb == NULL ||
        model->ub == NULL || model->vtype == NULL || model->solution == NULL) {
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
 * @param vtype Variable type ('C', 'B', 'I', 'S', 'N')
 * @param name Variable name (ignored in stub)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addvar(CxfModel *model, double lb, double ub, double obj,
               char vtype, const char *name) {
    int idx;

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
    model->vtype[idx] = vtype;
    model->solution[idx] = 0.0;
    model->num_vars++;

    return CXF_OK;
}

/**
 * @brief Add multiple variables to the model in batch (stub).
 *
 * This stub supports adding up to INITIAL_VAR_CAPACITY total variables.
 *
 * @param model Target model
 * @param numvars Number of variables to add
 * @param numnz Total non-zero coefficients (unused in stub)
 * @param vbeg Column start indices (unused in stub)
 * @param vind Constraint indices (unused in stub)
 * @param vval Coefficient values (unused in stub)
 * @param obj Objective coefficients (NULL = all 0.0)
 * @param lb Lower bounds (NULL = all 0.0)
 * @param ub Upper bounds (NULL = all infinity)
 * @param vtype Variable types (unused in stub)
 * @param varnames Variable names (unused in stub)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addvars(CxfModel *model, int numvars, int numnz,
                const int *vbeg, const int *vind, const double *vval,
                const double *obj, const double *lb, const double *ub,
                const char *vtype, const char **varnames) {
    (void)numnz;
    (void)vbeg;
    (void)vind;
    (void)vval;
    (void)vtype;
    (void)varnames;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (numvars <= 0) {
        return CXF_OK;
    }

    if (model->num_vars + numvars > INITIAL_VAR_CAPACITY) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < numvars; i++) {
        int idx = model->num_vars;
        model->obj_coeffs[idx] = (obj != NULL) ? obj[i] : 0.0;
        model->lb[idx] = (lb != NULL) ? lb[i] : 0.0;
        model->ub[idx] = (ub != NULL) ? ub[i] : CXF_INFINITY;
        model->solution[idx] = 0.0;
        model->num_vars++;
    }

    return CXF_OK;
}

/**
 * @brief Mark variables for deletion (stub).
 *
 * This stub validates inputs but doesn't actually remove variables.
 * Full implementation will use pending buffer with deletion mask.
 *
 * @param model Target model
 * @param numdel Number of variables to delete
 * @param ind Indices of variables to delete
 * @return CXF_OK on success, error code otherwise
 */
int cxf_delvars(CxfModel *model, int numdel, const int *ind) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (numdel <= 0) {
        return CXF_OK;
    }

    if (ind == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Validate all indices */
    for (int i = 0; i < numdel; i++) {
        if (ind[i] < 0 || ind[i] >= model->num_vars) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    }

    /* Stub: just validates, doesn't actually delete */
    return CXF_OK;
}
