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

/* Forward declaration - implemented in src/api/optimize_api.c */
extern int cxf_optimize_internal(CxfModel *model);

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

    /* Delegate to internal optimization dispatcher */
    return cxf_optimize_internal(model);
}

/* cxf_getintattr and cxf_getdblattr moved to attrs_api.c (M8.1.15) */

/**
 * @brief Get constraint data in CSR format (stub).
 *
 * @param model Model to query
 * @param numnzP Output: total nonzero count
 * @param cbeg Output: CSR row start indices (may be NULL)
 * @param cind Output: variable indices (may be NULL)
 * @param cval Output: coefficient values (may be NULL)
 * @param start First constraint index
 * @param len Number of constraints
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getconstrs(CxfModel *model, int *numnzP, int *cbeg,
                   int *cind, double *cval, int start, int len) {
    (void)cbeg;
    (void)cind;
    (void)cval;
    (void)start;
    (void)len;

    if (model == NULL || numnzP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Stub: return 0 nonzeros */
    *numnzP = 0;
    return CXF_OK;
}

/**
 * @brief Get a single coefficient from constraint matrix (stub).
 *
 * @param model Model to query
 * @param constr Constraint index (row)
 * @param var Variable index (column)
 * @param valP Output: coefficient value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getcoeff(CxfModel *model, int constr, int var, double *valP) {
    (void)constr;
    (void)var;

    if (model == NULL || valP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Stub: return 0.0 (sparse matrix convention) */
    *valP = 0.0;
    return CXF_OK;
}
