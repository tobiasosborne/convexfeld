/**
 * @file solve_lp_stub.c
 * @brief Stub implementation of simplex LP solver entry point.
 *
 * Provides a trivial unconstrained LP solver for the tracer bullet.
 * Full simplex implementation comes in M7 (Simplex Engine).
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Solve an LP using the simplex method (stub).
 *
 * This is a stub implementation that returns CXF_ERROR_NOT_SUPPORTED
 * for non-empty models. The real simplex solver is not yet implemented.
 *
 * Full implementation will handle:
 *   - Constraint matrix A
 *   - Phase I / Phase II simplex
 *   - Basis management
 *   - Pricing strategies
 *
 * @param model Model to solve (must have variables added)
 * @return CXF_OK for empty models, CXF_ERROR_NOT_SUPPORTED for non-empty,
 *         CXF_ERROR_NULL_ARGUMENT if model is NULL
 *
 * @pre model != NULL
 * @pre model->num_vars >= 0
 *
 * @post For empty models: model->status set to CXF_OPTIMAL, obj_val = 0.0
 * @post For non-empty models: returns NOT_SUPPORTED error
 */
int cxf_solve_lp(CxfModel *model) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Handle empty model */
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OK;
    }

    /* Stub: Real simplex not implemented yet */
    return CXF_ERROR_NOT_SUPPORTED;
}
