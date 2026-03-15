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
 * @brief Check if model contains any MIP-qualifying elements.
 *
 * Per V2 model_type_checking.md: checks MIP solve flag, integer/binary vars,
 * SOS constraints, indicator constraints, semi-continuous/semi-integer vars,
 * and other MIP indicators. Returns 1 if any MIP element is found.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model is MIP, 0 if pure continuous or NULL
 */
int cxf_is_mip_model(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* V2: Check MIP solve flag (solve_mode nonzero) */
    if (model->solve_mode != 0) {
        return 1;
    }

    /* V2: Check SOS constraint data (non-NULL means SOS present) */
    if (model->sos_data != NULL) {
        return 1;
    }

    /* V2: Check general constraint data (indicators, etc.) */
    if (model->gen_constr_data != NULL) {
        return 1;
    }

    /*
     * V2 TODO: When MatrixData gains these fields, check them here:
     * - piecewise-linear objective term count
     * - quadratic constraint count (may indicate non-convex MIQCP)
     * - multi-objective / scenario optimization flag
     * - force-non-convex flag
     */

    /* Check variable types for any non-continuous variable */
    if (model->num_vars > 0 && model->vtype != NULL) {
        for (int i = 0; i < model->num_vars; i++) {
            char vt = model->vtype[i];
            if (vt != 'C' && vt != CXF_CONTINUOUS) {
                return 1;
            }
        }
    }

    return 0;  /* Pure continuous */
}

/**
 * @brief Check if model is a Quadratic Program (QP).
 *
 * Determines if the model has a quadratic objective without disqualifying
 * features (quadratic constraints, bilinear terms, etc.).
 *
 * Note: Currently returns 0 as quadratic objective fields are not yet
 * implemented in the MatrixData structure.
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
 * in the MatrixData structure.
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
