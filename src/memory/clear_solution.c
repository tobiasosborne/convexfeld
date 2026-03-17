/**
 * @file clear_solution.c
 * @brief Clear all solution data from a model (state_cleanup_buffers.md)
 *
 * Resets the model to a pre-solve state ready for re-optimization.
 * Solution arrays are zeroed (not freed) so they can be reused.
 *
 * Spec: docs/specs-v2/specs/modules/state_cleanup_buffers.md
 *       Function: cxf_clear_solution
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

#include "memory_internal.h"

/**
 * @brief Clear all solution-related data from a model.
 *
 * Zeroes solution arrays (primal x, dual pi), resets status to
 * CXF_LOADED, resets objective value to 0.0. Does NOT free the
 * arrays -- they are reused on the next solve.
 *
 * Per spec: if clearHints is nonzero, also clears extended data
 * (solution_data, sos_data, gen_constr_data) and resets fingerprint.
 *
 * @param model  Model to clear (NULL is a no-op returning CXF_OK)
 * @param clearHints  If nonzero, also clear warm-start/hint data
 * @return CXF_OK on success, error code on failure
 */
int cxf_clear_solution(CxfModel *model, int clearHints) {
    if (model == NULL) {
        return CXF_OK;
    }

    /* Zero primal solution array if it exists */
    if (model->solution != NULL && model->num_vars > 0) {
        memset(model->solution, 0,
               (size_t)model->num_vars * sizeof(double));
    }

    /* Zero dual values array if it exists */
    if (model->pi != NULL && model->num_constrs > 0) {
        memset(model->pi, 0,
               (size_t)model->num_constrs * sizeof(double));
    }

    /* Reset solution status to "not solved" */
    model->status = CXF_LOADED;

    /* Reset objective value */
    model->obj_val = 0.0;

    /* Mark model as initialized (ready for re-optimization) */
    model->initialized = 1;

    /* Clear extended data when clearHints is requested */
    if (clearHints) {
        /* Free and null extended solution data */
        cxf_free(model->solution_data);
        model->solution_data = NULL;

        /* Free and null SOS constraint data */
        cxf_free(model->sos_data);
        model->sos_data = NULL;

        /* Free and null general constraint data */
        cxf_free(model->gen_constr_data);
        model->gen_constr_data = NULL;

        /* Clear fingerprint */
        model->fingerprint = 0;
    }

    return CXF_OK;
}
