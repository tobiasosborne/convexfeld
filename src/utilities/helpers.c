/**
 * @file helpers.c
 * @brief Utility helper functions (M7.3.3)
 *
 * Provides miscellaneous utility functions for model inspection
 * and constraint analysis.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include <stddef.h>

/**
 * @brief Check if model has multiple objectives.
 *
 * Determines whether a model uses multi-objective optimization (NumObj > 1).
 * Multi-objective models support hierarchical or blended optimization with
 * multiple objective functions.
 *
 * Note: Currently returns 0 as multi-objective fields are not yet implemented
 * in the SparseMatrix structure. This is a stub implementation that will be
 * completed when multi-objective support is added.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has multiple objectives (NumObj > 1), 0 otherwise
 */
int cxf_is_multi_objective(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* No matrix means no multi-objective data */
    if (model->matrix == NULL) {
        return 0;
    }

    /*
     * Note: Multi-objective detection would check:
     *
     * if (model->matrix != NULL) {
     *     if (model->matrix->numObjectives > 1) {
     *         return 1;  // Multi-objective
     *     }
     * }
     *
     * For now, this field is not yet implemented in SparseMatrix.
     * Default to single objective (standard LP case).
     */

    return 0;  /* Single objective (default) */
}
