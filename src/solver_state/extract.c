/**
 * @file extract.c
 * @brief Extract solution from solver state to model
 *
 * Implementation of cxf_extract_solution which copies primal values,
 * dual values, and objective value from the solver's working arrays
 * to the model's solution arrays.
 *
 * Spec: docs/specs/functions/simplex/cxf_extract_solution.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/* External memory allocation functions */
extern void *cxf_malloc(size_t size);

/**
 * @brief Extract solution from solver state to model.
 *
 * Copies primal solution (x), dual values (pi), and objective value
 * from the solver's working arrays to the model's solution arrays.
 * Allocates solution arrays if they are NULL.
 *
 * @param state Solver context with solution (non-NULL)
 * @param model Model to receive solution (non-NULL)
 * @return CXF_OK on success, error code otherwise
 *
 * @retval CXF_OK               Solution extracted successfully
 * @retval CXF_ERROR_NULL_ARGUMENT  state or model is NULL
 * @retval CXF_ERROR_OUT_OF_MEMORY  Allocation failed
 */
int cxf_extract_solution(SolverContext *state, CxfModel *model) {
    /* Validate inputs */
    if (state == NULL || model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int n = state->num_vars;
    int m = state->num_constrs;

    /* Step 1: Allocate and copy primal solution */
    if (n > 0) {
        /* Allocate solution array if needed */
        if (model->solution == NULL) {
            model->solution = (double *)cxf_malloc((size_t)n * sizeof(double));
            if (model->solution == NULL) {
                return CXF_ERROR_OUT_OF_MEMORY;
            }
        }

        /* Copy primal values from working array */
        if (state->work_x != NULL) {
            memcpy(model->solution, state->work_x, (size_t)n * sizeof(double));
        } else {
            /* No solution available - zero out array */
            memset(model->solution, 0, (size_t)n * sizeof(double));
        }
    }

    /* Step 2: Allocate and copy dual values */
    if (m > 0) {
        /* Allocate dual array if needed */
        if (model->pi == NULL) {
            model->pi = (double *)cxf_malloc((size_t)m * sizeof(double));
            if (model->pi == NULL) {
                return CXF_ERROR_OUT_OF_MEMORY;
            }
        }

        /* Copy dual values from working array */
        if (state->work_pi != NULL) {
            memcpy(model->pi, state->work_pi, (size_t)m * sizeof(double));
        } else {
            /* No duals available - zero out array */
            memset(model->pi, 0, (size_t)m * sizeof(double));
        }
    }

    /* Step 3: Set objective value */
    model->obj_val = state->obj_value;

    /* Step 4: Set status based on solver phase */
    /* If phase 2 completed, mark as optimal, otherwise keep current status */
    if (state->phase == 2) {
        model->status = CXF_OPTIMAL;
    }
    /* Note: caller may override status based on termination condition */

    return CXF_OK;
}
