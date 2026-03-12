/**
 * @file presolve_stats.c
 * @brief Model statistics logging (M4.3.4)
 *
 * Logs descriptive statistics about model features before optimization.
 * Reports quadratic terms, SOS constraints, PWL objectives, and general
 * constraints when present.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"

/* Forward declaration for logging */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);

/**
 * @brief Log model statistics before optimization.
 *
 * Reports counts of advanced model features:
 * - Quadratic objective terms
 * - Quadratic constraints
 * - Bilinear constraints
 * - SOS constraints
 * - Piecewise-linear objective terms
 * - General constraints by type
 *
 * For pure LP models, logs basic dimensions only.
 *
 * @param model Model to analyze
 */
void cxf_presolve_stats(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return;
    }

    CxfEnv *env = model->env;

    /* Basic LP dimensions - always log at verbose level */
    int64_t nnz = 0;
    if (model->matrix != NULL) {
        nnz = model->matrix->nnz;
    }

    cxf_log_printf(env, 2, "Model '%s': %d variable%s, %d constraint%s, %lld nonzero%s",
                   model->name[0] ? model->name : "(unnamed)",
                   model->num_vars, model->num_vars == 1 ? "" : "s",
                   model->num_constrs, model->num_constrs == 1 ? "" : "s",
                   (long long)nnz, nnz == 1 ? "" : "s");

    /* Advanced feature logging (quadratic, SOS, general constraints, etc.)
     * will be added when CxfModel gains these fields. Currently pure LP only. */
}
