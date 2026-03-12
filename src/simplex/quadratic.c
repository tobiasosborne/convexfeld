/**
 * @file quadratic.c
 * @brief Quadratic programming reduced cost adjustment (stub)
 *
 * Stub: Full QP support (Q matrix) is not yet implemented in CxfModel.
 * This file provides the interface and input validation for future
 * expansion. When QP is implemented, this function will compute
 * gradient contributions Qx for reduced cost updates.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

/**
 * @brief Adjust reduced costs for quadratic programming.
 *
 * Stub: validates inputs and returns CXF_OK. No-op until Q matrix
 * support is added to CxfModel.
 *
 * @param state Solver context with QP data and solution
 * @param varIndex Variable index to adjust (-1 for all nonbasic variables)
 * @return CXF_OK on success, error code on invalid input
 */
int cxf_quadratic_adjust(SolverState *state, int varIndex) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (varIndex >= 0) {
        if (varIndex >= state->num_vars) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
    } else if (varIndex != -1) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Stub: QP not yet implemented — nothing to adjust */
    return CXF_OK;
}
