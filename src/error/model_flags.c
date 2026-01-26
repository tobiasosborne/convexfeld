/**
 * @file model_flags.c
 * @brief Model flag check functions (M3.1.5)
 *
 * Functions to determine model characteristics for solver dispatch:
 * - cxf_check_model_flags1: Detects MIP features (integer vars, SOS, etc.)
 * - cxf_check_model_flags2: Detects quadratic/conic features
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"

/**
 * @brief Check if model contains MIP (Mixed-Integer Programming) features.
 *
 * Examines the model for features requiring branch-and-bound algorithms:
 * - Integer-type variables (Binary, Integer, Semi-continuous, Semi-integer)
 * - SOS (Special Ordered Set) constraints
 * - General constraints (AND, OR, INDICATOR, etc.)
 *
 * Used during solver dispatch to select appropriate algorithms.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has MIP features, 0 if pure continuous or NULL
 */
int cxf_check_model_flags1(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* Check for integer-type variables */
    if (model->vtype != NULL && model->num_vars > 0) {
        for (int i = 0; i < model->num_vars; i++) {
            char vt = model->vtype[i];
            /* Non-continuous types: Binary, Integer, Semi-cont, Semi-int */
            if (vt != 'C' && vt != CXF_CONTINUOUS) {
                return 1;  /* Found MIP variable */
            }
        }
    }

    /*
     * Note: SOS constraints and general constraints would be checked here
     * when the matrix structure includes those fields:
     *
     * if (model->matrix != NULL) {
     *     if (model->matrix->sosCount > 0) return 1;
     *     if (model->matrix->genConstrCount > 0) return 1;
     * }
     *
     * For now, these fields are not yet implemented in SparseMatrix.
     */

    return 0;  /* Pure continuous model */
}

/**
 * @brief Check if model contains quadratic or conic features.
 *
 * Examines the model for features requiring barrier (interior point) methods:
 * - Quadratic objective terms (QP)
 * - Quadratic constraints (QCP)
 * - Bilinear terms
 * - Second-order cone constraints (SOCP)
 * - Rotated cone, exponential cone, power cone constraints
 *
 * The flag parameter allows future extension for specific feature checks.
 *
 * @param model Model to check (may be NULL)
 * @param flag Reserved for future use (currently ignored)
 * @return 1 if model has quadratic/conic features, 0 if pure linear or NULL
 */
int cxf_check_model_flags2(CxfModel *model, int flag) {
    (void)flag;  /* Reserved for future use */

    if (model == NULL) {
        return 0;
    }

    /*
     * Note: Quadratic and conic features would be checked here when
     * the matrix structure includes those fields:
     *
     * if (model->matrix != NULL) {
     *     if (model->matrix->quadObjTerms > 0) return 1;
     *     if (model->matrix->quadConstrCount > 0) return 1;
     *     if (model->matrix->bilinearCount > 0) return 1;
     *     if (model->matrix->socCount > 0) return 1;
     *     if (model->matrix->rotatedConeCount > 0) return 1;
     *     if (model->matrix->expConeCount > 0) return 1;
     *     if (model->matrix->powConeCount > 0) return 1;
     *     if (model->matrix->nlConstrCount > 0) return 1;
     * }
     *
     * For now, these fields are not yet implemented in SparseMatrix.
     * The current implementation returns 0 (pure linear) for all models.
     */

    return 0;  /* Pure linear model */
}
