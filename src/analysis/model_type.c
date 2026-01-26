/**
 * @file model_type.c
 * @brief Model type classification functions (M4.3.2)
 *
 * Implements model type detection functions:
 * - cxf_is_mip_model: Check for integer variables (MIP)
 * - cxf_is_quadratic: Check for quadratic objective (QP)
 * - cxf_is_socp: Check for SOCP/QCP features
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include <stddef.h>

/**
 * @brief Check if model contains integer-type variables (MIP).
 *
 * Scans the model's variable type array for any non-continuous variable.
 * Returns immediately upon finding the first integer-type variable.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has integer variables, 0 if all continuous or NULL
 */
int cxf_is_mip_model(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* No variables means no integer variables */
    if (model->num_vars <= 0) {
        return 0;
    }

    /* NULL vtype means all continuous (default) */
    if (model->vtype == NULL) {
        return 0;
    }

    /* Scan for any non-continuous variable */
    for (int i = 0; i < model->num_vars; i++) {
        char vt = model->vtype[i];
        /* Non-continuous types: Binary(B), Integer(I), Semi-cont(S), Semi-int(N) */
        if (vt != 'C' && vt != CXF_CONTINUOUS) {
            return 1;  /* Found integer variable */
        }
    }

    return 0;  /* All continuous */
}

/**
 * @brief Check if model is a Quadratic Program (QP).
 *
 * Determines if the model has a quadratic objective without disqualifying
 * features (quadratic constraints, bilinear terms, etc.).
 *
 * Note: Currently returns 0 as quadratic objective fields are not yet
 * implemented in the SparseMatrix structure.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model is QP, 0 otherwise or NULL
 */
int cxf_is_quadratic(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /*
     * Note: Quadratic objective detection would check:
     *
     * if (model->matrix != NULL) {
     *     if (model->matrix->quadObjTerms > 0) {
     *         // Check for disqualifying features
     *         if (model->matrix->quadConstrCount > 0) return 0;
     *         if (model->matrix->bilinearCount > 0) return 0;
     *         return 1;  // Pure QP
     *     }
     * }
     *
     * For now, these fields are not yet implemented.
     */

    return 0;  /* Pure linear (no quadratic objective) */
}

/**
 * @brief Check if model has SOCP/QCP features.
 *
 * Examines the model for second-order cone, quadratic constraints,
 * bilinear terms, and other conic features that require barrier methods.
 *
 * Note: Currently returns 0 as SOCP/QCP fields are not yet implemented
 * in the SparseMatrix structure.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has SOCP/QCP features, 0 if pure linear or NULL
 */
int cxf_is_socp(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /*
     * Note: SOCP/QCP detection would check:
     *
     * if (model->matrix != NULL) {
     *     if (model->matrix->qcpConstrCount > 0) return 1;
     *     if (model->matrix->bilinearCount > 0) return 1;
     *     if (model->matrix->socConstrCount > 0) return 1;
     *     if (model->matrix->rotatedConeCount > 0) return 1;
     *     if (model->matrix->expConeCount > 0) return 1;
     *     if (model->matrix->powConeCount > 0) return 1;
     * }
     *
     * For now, these fields are not yet implemented.
     */

    return 0;  /* Pure linear (no SOCP/QCP features) */
}
