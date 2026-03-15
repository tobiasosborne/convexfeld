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
 * @brief Check if model qualifies as a pure Quadratic Program (QP).
 *
 * Per V2 model_type_checking.md: returns 1 if the Environment has a
 * force-QP parameter set (override), OR if general constraints are
 * present AND no disqualifying elements (binary vars, indicator
 * constraints, semi-continuous/semi-integer vars, NLP vars, nonlinear
 * elements, multi-scenario).
 *
 * If no general constraints are present, returns 0 (pure LP).
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model is a pure QP, 0 otherwise or NULL
 */
int cxf_is_quadratic(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* V2 path 1: force-QP env parameter override.
     * TODO: When CxfEnv gains a force_qp field, check it here:
     *   if (model->env != NULL && model->env->force_qp) return 1;
     */

    /* V2 path 2: general constraints present, no disqualifiers */
    if (model->gen_constr_data == NULL) {
        return 0;  /* No general constraints -> pure LP, not QP */
    }

    /* Disqualifier: binary variables */
    if (model->num_vars > 0 && model->vtype != NULL) {
        for (int i = 0; i < model->num_vars; i++) {
            char vt = model->vtype[i];
            if (vt == 'B' || vt == CXF_BINARY) return 0;
            if (vt == 'S' || vt == CXF_SEMICONT) return 0;
            if (vt == 'N' || vt == CXF_SEMIINT) return 0;
        }
    }

    /* Disqualifier: SOS/indicator constraints (sos_data doubles as
     * indicator presence check per current data model) */
    if (model->sos_data != NULL) {
        return 0;
    }

    /* TODO: When MatrixData gains these fields, check:
     * - indicator constraint count
     * - NLP variable count (when NLP mode enabled in env)
     * - other nonlinear element count
     * - multi-scenario configuration count
     */

    return 1;  /* General constraints present, no disqualifiers */
}

/**
 * @brief Check if model has any SOCP-related elements.
 *
 * Per V2 model_type_checking.md: returns 1 if ANY SOCP presence
 * indicator is found. This is the public/reporting variant — all
 * checked fields are treated as presence indicators (no
 * disqualifier logic). Used for model statistics, not solver routing.
 *
 * Indicators checked: force-SOCP env param, general constraints
 * (which may contain quadratic/cone/indicator constraints),
 * SOS data (which may encode cone structure), semi-continuous/
 * semi-integer variables.
 *
 * @param model Model to check (may be NULL)
 * @return 1 if model has SOCP/QCP features, 0 if pure linear or NULL
 */
int cxf_is_socp(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* V2: force-SOCP env parameter override.
     * TODO: When CxfEnv gains a force_socp field, check it here:
     *   if (model->env != NULL && model->env->force_socp) return 1;
     */

    /* V2: general constraint data signals quadratic/cone/indicator
     * constraints may be present */
    if (model->gen_constr_data != NULL) {
        return 1;
    }

    /* V2: SOS data may encode cone structure */
    if (model->sos_data != NULL) {
        return 1;
    }

    /* V2: semi-continuous or semi-integer variables */
    if (model->num_vars > 0 && model->vtype != NULL) {
        for (int i = 0; i < model->num_vars; i++) {
            char vt = model->vtype[i];
            if (vt == 'S' || vt == CXF_SEMICONT) return 1;
            if (vt == 'N' || vt == CXF_SEMIINT) return 1;
        }
    }

    /* TODO: When MatrixData gains these fields, also check:
     * - quadratic constraint count
     * - explicit cone constraint count
     * - indicator constraint count
     * - multi-scenario configuration count
     * - NLP variable count (when NLP mode enabled)
     * - other nonlinear element count
     */

    return 0;  /* Pure linear (no SOCP/QCP features) */
}
