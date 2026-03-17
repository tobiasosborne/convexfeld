/**
 * @file api_stub.c
 * @brief Stub optimization and attribute functions for tracer bullet.
 *
 * Minimal implementation of cxf_optimize and attribute getters.
 * Delegates actual solving to cxf_solve_lp() in simplex module.
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Forward declaration - implemented in src/api/optimize_api.c */

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
    int status;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Step 3 (solve_entry.md): Acquire locale safety state.
     * Save the calling thread's LC_NUMERIC locale and switch to "C"
     * to ensure consistent decimal point formatting throughout
     * optimization (e.g., MPS parsing uses '.' not ','). */
    cxf_acquire_solve_lock(model->env);

    /* Step 4 (solve_entry.md): Set modification_blocked to prevent
     * concurrent modifications during optimization */
    model->modification_blocked = 1;

    /* Delegate to internal optimization dispatcher */
    status = cxf_optimize_internal(model);

    /* Step 12 (solve_entry.md): Clear modification_blocked on all paths */
    model->modification_blocked = 0;

    /* Step 12 (solve_entry.md): Release locale safety state.
     * Restore the original LC_NUMERIC locale. */
    cxf_release_solve_lock(model->env);

    return status;
}

/* cxf_getintattr and cxf_getdblattr moved to attrs_api.c (M8.1.15) */

/**
 * @brief Get constraint data in CSR format (not yet implemented).
 *
 * @param model Model to query
 * @param numnzP Output: total nonzero count
 * @param cbeg Output: CSR row start indices (may be NULL)
 * @param cind Output: variable indices (may be NULL)
 * @param cval Output: coefficient values (may be NULL)
 * @param start First constraint index
 * @param len Number of constraints
 * @return CXF_ERROR_NOT_SUPPORTED (not yet implemented)
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

    return CXF_ERROR_NOT_SUPPORTED;
}

/**
 * @brief Get a single coefficient from constraint matrix (not yet implemented).
 *
 * @param model Model to query
 * @param constr Constraint index (row)
 * @param var Variable index (column)
 * @param valP Output: coefficient value
 * @return CXF_ERROR_NOT_SUPPORTED (not yet implemented)
 */
int cxf_getcoeff(CxfModel *model, int constr, int var, double *valP) {
    (void)constr;
    (void)var;

    if (model == NULL || valP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    return CXF_ERROR_NOT_SUPPORTED;
}
