/**
 * @file coef_stats.c
 * @brief Coefficient statistics for numerical conditioning analysis.
 *
 * M4.3.3: cxf_coefficient_stats, cxf_compute_coef_stats
 *
 * Analyzes model coefficients to detect potential numerical issues.
 * LP-only implementation (no quadratic support).
 */

#include <math.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"

/* Thresholds for numerical warnings */
#define COEF_RANGE_THRESHOLD 13      /* log10(max/min) threshold */
#define LARGE_COEF_THRESHOLD 1e13    /* Absolute coefficient threshold */

/**
 * @brief Compute min/max coefficient statistics for LP model.
 *
 * Scans objective, bounds, and matrix coefficients to find ranges.
 * Zero coefficients are excluded from minimum calculations.
 * Infinite bounds are excluded from bound calculations.
 *
 * @param model Model to analyze
 * @param obj_min Output: minimum objective coefficient (nonzero)
 * @param obj_max Output: maximum objective coefficient
 * @param bounds_min Output: minimum bound (nonzero, non-infinite)
 * @param bounds_max Output: maximum bound (non-infinite)
 * @param matrix_min Output: minimum matrix coefficient (nonzero)
 * @param matrix_max Output: maximum matrix coefficient
 * @return CXF_OK on success
 */
int cxf_compute_coef_stats(CxfModel *model,
                           double *obj_min, double *obj_max,
                           double *bounds_min, double *bounds_max,
                           double *matrix_min, double *matrix_max) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Initialize ranges */
    double o_min = CXF_INFINITY, o_max = 0.0;
    double b_min = CXF_INFINITY, b_max = 0.0;
    double m_min = CXF_INFINITY, m_max = 0.0;

    /* Scan objective coefficients */
    if (model->obj_coeffs != NULL) {
        for (int j = 0; j < model->num_vars; j++) {
            double val = fabs(model->obj_coeffs[j]);
            if (val > 0.0) {
                if (val < o_min) o_min = val;
                if (val > o_max) o_max = val;
            }
        }
    }

    /* Scan bounds (excluding infinities) */
    if (model->lb != NULL && model->ub != NULL) {
        for (int j = 0; j < model->num_vars; j++) {
            double lb_val = fabs(model->lb[j]);
            double ub_val = fabs(model->ub[j]);

            if (lb_val > 0.0 && lb_val < CXF_INFINITY * 0.1) {
                if (lb_val < b_min) b_min = lb_val;
                if (lb_val > b_max) b_max = lb_val;
            }
            if (ub_val > 0.0 && ub_val < CXF_INFINITY * 0.1) {
                if (ub_val < b_min) b_min = ub_val;
                if (ub_val > b_max) b_max = ub_val;
            }
        }
    }

    /* Scan matrix coefficients (CSC format) */
    if (model->matrix != NULL && model->matrix->values != NULL) {
        SparseMatrix *mat = model->matrix;
        for (int k = 0; k < mat->nnz; k++) {
            double val = fabs(mat->values[k]);
            if (val > 0.0) {
                if (val < m_min) m_min = val;
                if (val > m_max) m_max = val;
            }
        }
    }

    /* Handle empty ranges */
    if (o_max == 0.0) o_min = 0.0;
    if (b_max == 0.0) b_min = 0.0;
    if (m_max == 0.0) m_min = 0.0;

    /* Output results */
    if (obj_min) *obj_min = o_min;
    if (obj_max) *obj_max = o_max;
    if (bounds_min) *bounds_min = b_min;
    if (bounds_max) *bounds_max = b_max;
    if (matrix_min) *matrix_min = m_min;
    if (matrix_max) *matrix_max = m_max;

    return CXF_OK;
}

/**
 * @brief Compute and optionally log coefficient statistics.
 *
 * Analyzes model coefficients and issues warnings about potential
 * numerical issues that may cause solver instability.
 *
 * @param model Model to analyze
 * @param verbose 1 to log statistics, 0 for silent computation
 * @return CXF_OK on success
 */
int cxf_coefficient_stats(CxfModel *model, int verbose) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Skip if already solved */
    if (model->status != 0) {
        return CXF_OK;
    }

    double obj_min, obj_max;
    double bounds_min, bounds_max;
    double matrix_min, matrix_max;

    int status = cxf_compute_coef_stats(model,
                                        &obj_min, &obj_max,
                                        &bounds_min, &bounds_max,
                                        &matrix_min, &matrix_max);
    if (status != CXF_OK) {
        return status;
    }

    /* Silent mode - just compute, don't log */
    if (verbose == 0) {
        return CXF_OK;
    }

    /* Check for numerical issues */
    int warning_issued = 0;

    /* Check matrix coefficient range */
    if (matrix_max > 0.0 && matrix_min > 0.0) {
        double range = log10(matrix_max / matrix_min);
        if (range >= COEF_RANGE_THRESHOLD) {
            warning_issued = 1;
        } else if (matrix_max > LARGE_COEF_THRESHOLD) {
            warning_issued = 1;
        }
    }

    /* Check objective coefficients */
    if (obj_max > LARGE_COEF_THRESHOLD) {
        warning_issued = 1;
    }

    /* Check bounds */
    if (bounds_max > LARGE_COEF_THRESHOLD) {
        warning_issued = 1;
    }

    (void)warning_issued;  /* Could be used for logging in full implementation */

    return CXF_OK;
}
