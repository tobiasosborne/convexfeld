/**
 * @file model_stub.c
 * @brief Stub variable manipulation functions.
 *
 * Minimal implementation of variable addition and deletion.
 * Full implementation with dynamic resizing comes in M8.1.11.
 *
 * Note: cxf_newmodel and cxf_freemodel are in model.c (M8.1.8).
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/**
 * @brief Add a single variable to the model (stub).
 *
 * Uses var_capacity for bounds checking.
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

    if (model->num_vars >= model->var_capacity) {
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
 * Uses var_capacity for bounds checking.
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

    if (model->num_vars + numvars > model->var_capacity) {
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
