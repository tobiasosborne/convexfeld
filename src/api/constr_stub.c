/**
 * @file constr_stub.c
 * @brief Stub constraint functions for tracer bullet.
 *
 * Minimal implementation of constraint addition.
 * Full implementation with pending buffer management comes later.
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Forward declare memory functions */
extern void *cxf_calloc(size_t count, size_t size);
extern void *cxf_malloc(size_t size);
extern void cxf_free(void *ptr);

/* Initial capacity for constraint tracking */
#define INITIAL_CONSTR_CAPACITY 16

/**
 * @brief Add a single linear constraint (stub).
 *
 * @param model Target model
 * @param numnz Number of non-zero coefficients
 * @param cind Variable indices
 * @param cval Coefficient values
 * @param sense Constraint sense ('<', '>', '=')
 * @param rhs Right-hand side value
 * @param constrname Constraint name (may be NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname) {
    (void)numnz;
    (void)cind;
    (void)cval;
    (void)sense;
    (void)rhs;
    (void)constrname;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (model->num_constrs >= INITIAL_CONSTR_CAPACITY) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Stub: just increment counter, no actual storage */
    model->num_constrs++;
    return CXF_OK;
}

/**
 * @brief Add multiple linear constraints in batch (stub).
 *
 * @param model Target model
 * @param numconstrs Number of constraints to add
 * @param numnz Total non-zero coefficients
 * @param cbeg CSR row start indices
 * @param cind Variable indices
 * @param cval Coefficient values
 * @param sense Constraint senses
 * @param rhs Right-hand side values
 * @param constrnames Constraint names (may be NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addconstrs(CxfModel *model, int numconstrs, int numnz,
                   const int *cbeg, const int *cind, const double *cval,
                   const char *sense, const double *rhs,
                   const char **constrnames) {
    (void)numnz;
    (void)cbeg;
    (void)cind;
    (void)cval;
    (void)sense;
    (void)rhs;
    (void)constrnames;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (numconstrs <= 0) {
        return CXF_OK;
    }

    if (model->num_constrs + numconstrs > INITIAL_CONSTR_CAPACITY) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Stub: just increment counter, no actual storage */
    model->num_constrs += numconstrs;
    return CXF_OK;
}

/**
 * @brief Add a quadratic constraint (stub - not supported).
 *
 * @return CXF_ERROR_NOT_SUPPORTED always (LP solver only)
 */
int cxf_addqconstr(CxfModel *model, int numlnz, const int *lind,
                   const double *lval, int numqnz, const int *qrow,
                   const int *qcol, const double *qval, char sense,
                   double rhs, const char *name) {
    (void)model;
    (void)numlnz;
    (void)lind;
    (void)lval;
    (void)numqnz;
    (void)qrow;
    (void)qcol;
    (void)qval;
    (void)sense;
    (void)rhs;
    (void)name;

    return CXF_ERROR_NOT_SUPPORTED;
}

/**
 * @brief Add an indicator constraint (stub - not supported).
 *
 * @return CXF_ERROR_NOT_SUPPORTED always (LP solver only)
 */
int cxf_addgenconstrIndicator(CxfModel *model, const char *name,
                              int binvar, int binval, int nvars,
                              const int *ind, const double *val,
                              char sense, double rhs) {
    (void)model;
    (void)name;
    (void)binvar;
    (void)binval;
    (void)nvars;
    (void)ind;
    (void)val;
    (void)sense;
    (void)rhs;

    return CXF_ERROR_NOT_SUPPORTED;
}
