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

/* General constraint type names (for future expansion) */
static const char *genconstr_names[] = {
    "MAX", "MIN", "ABS", "AND", "OR", "NORM", "NL",
    "INDICATOR", "PWL", "POLY", "EXP", "EXPA", "LOG",
    "LOGA", "POW", "SIN", "COS", "TAN", "LOGISTIC"
};
#define NUM_GENCONSTR_TYPES 19

/**
 * @brief Get human-readable name for general constraint type.
 *
 * @param type Constraint type index (0-18)
 * @return Type name string, or "UNKNOWN" for invalid type
 */
static const char *get_genconstr_name(int type) {
    if (type >= 0 && type < NUM_GENCONSTR_TYPES) {
        return genconstr_names[type];
    }
    return "UNKNOWN";
}

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

    /*
     * Advanced features - currently not in CxfModel.
     * When these fields are added, the code below will activate.
     * For now, all counts are effectively zero.
     */

    /* Quadratic objective terms (future: model->quad_obj_terms) */
    int quad_obj_terms = 0;
    if (quad_obj_terms >= 1) {
        cxf_log_printf(env, 1, "Model has %d quadratic objective term%s",
                       quad_obj_terms, quad_obj_terms == 1 ? "" : "s");
    }

    /* Quadratic constraints (future: model->quad_constr_count) */
    int quad_constr_count = 0;
    if (quad_constr_count >= 1) {
        cxf_log_printf(env, 1, "Model has %d quadratic constraint%s",
                       quad_constr_count, quad_constr_count == 1 ? "" : "s");
    }

    /* Bilinear constraints (future: model->bilinear_count) */
    int bilinear_count = 0;
    if (bilinear_count >= 1) {
        cxf_log_printf(env, 1, "Model has %d bilinear constraint%s",
                       bilinear_count, bilinear_count == 1 ? "" : "s");
    }

    /* SOS constraints (future: model->sos_count) */
    int sos_count = 0;
    if (sos_count >= 1) {
        cxf_log_printf(env, 1, "Model has %d SOS constraint%s",
                       sos_count, sos_count == 1 ? "" : "s");
    }

    /* Piecewise-linear objective terms (future: model->pwl_obj_count) */
    int pwl_obj_count = 0;
    if (pwl_obj_count >= 1) {
        cxf_log_printf(env, 1, "Model has %d piecewise-linear objective term%s",
                       pwl_obj_count, pwl_obj_count == 1 ? "" : "s");
    }

    /* General constraints (future: model->genconstr_count, model->genconstr_types) */
    int genconstr_count = 0;
    if (genconstr_count == 0) {
        return;  /* No general constraints - done */
    }

    /* Count general constraints by type */
    int pwl_counts[NUM_GENCONSTR_TYPES] = {0};
    int nl_counts[NUM_GENCONSTR_TYPES] = {0};

    /* Future: iterate model->genconstr array and populate counts */

    /* Categorize into simple, PWL-approximated, and nonlinear */
    int simple_count = 0;
    int pwl_func_count = 0;
    int nl_func_count = 0;
    int nl_gencon_count = 0;

    for (int t = 0; t < NUM_GENCONSTR_TYPES; t++) {
        if (t >= 0 && t <= 8 && t != 6) {
            /* Simple constraints: MAX, MIN, ABS, AND, OR, NORM, INDICATOR, PWL */
            simple_count += pwl_counts[t] + nl_counts[t];
        } else if (t >= 9) {
            /* Function constraints */
            pwl_func_count += pwl_counts[t];
            nl_func_count += nl_counts[t];
        } else if (t == 6) {
            /* General nonlinear (NL) */
            nl_gencon_count += nl_counts[t];
        }
    }

    /* Log simple general constraints */
    if (simple_count > 0) {
        cxf_log_printf(env, 1, "Model has %d simple general constraint%s:",
                       simple_count, simple_count == 1 ? "" : "s");

        int items_on_line = 0;
        for (int t = 0; t <= 8; t++) {
            if (t == 6) continue;  /* Skip NL */
            int count = pwl_counts[t] + nl_counts[t];
            if (count > 0) {
                cxf_log_printf(env, 1, "  %s: %d", get_genconstr_name(t), count);
                items_on_line++;
                if (items_on_line >= 5) {
                    items_on_line = 0;
                }
            }
        }
    }

    /* Log PWL-approximated function constraints */
    if (pwl_func_count > 0) {
        cxf_log_printf(env, 1, "Model has %d function constraint%s approximated by PWL:",
                       pwl_func_count, pwl_func_count == 1 ? "" : "s");

        for (int t = 9; t < NUM_GENCONSTR_TYPES; t++) {
            if (pwl_counts[t] > 0) {
                cxf_log_printf(env, 1, "  %s: %d", get_genconstr_name(t), pwl_counts[t]);
            }
        }
    }

    /* Log nonlinear function constraints */
    if (nl_func_count > 0) {
        cxf_log_printf(env, 1, "Model has %d function constraint%s treated as nonlinear:",
                       nl_func_count, nl_func_count == 1 ? "" : "s");

        for (int t = 9; t < NUM_GENCONSTR_TYPES; t++) {
            if (nl_counts[t] > 0) {
                cxf_log_printf(env, 1, "  %s: %d", get_genconstr_name(t), nl_counts[t]);
            }
        }
    }

    /* Log general nonlinear constraints */
    if (nl_gencon_count > 0) {
        int nl_term_count = 0;  /* Future: model->nl_term_count */
        cxf_log_printf(env, 1, "Model has %d general nonlinear constraint%s (%d nonlinear term%s)",
                       nl_gencon_count, nl_gencon_count == 1 ? "" : "s",
                       nl_term_count, nl_term_count == 1 ? "" : "s");
    }
}
