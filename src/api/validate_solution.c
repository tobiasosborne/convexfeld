/**
 * @file validate_solution.c
 * @brief Solution validation per V2 data_validation.md cxf_validate_solution.
 *
 * Checks variable bounds and linear constraint satisfaction against a
 * tolerance. Reports the total number of violations.
 *
 * Phases implemented:
 *   1. Variable bound feasibility
 *   2. Linear constraint feasibility (LE, GE, EQ via CSC multiply)
 *
 * Phases not yet implemented (filed as future work):
 *   - Quadratic constraint feasibility
 *   - Indicator constraint feasibility
 *   - Integrality feasibility
 *   - SOS constraint feasibility
 *   - General constraint feasibility
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include "../memory/memory_internal.h"
#include <math.h>

/**
 * @brief Count variable bound violations.
 *
 * For each variable i, checks:
 *   solution[i] >= lb[i] - tolerance
 *   solution[i] <= ub[i] + tolerance
 *
 * @return number of bound violations
 */
static int check_bounds(const CxfModel *model, double tolerance) {
    int violations = 0;
    for (int i = 0; i < model->num_vars; i++) {
        double x = model->solution[i];
        double lo = model->lb[i];
        double hi = model->ub[i];

        /* Skip infinite bounds */
        if (lo > -CXF_INFINITY && x < lo - tolerance) {
            violations++;
        }
        if (hi < CXF_INFINITY && x > hi + tolerance) {
            violations++;
        }
    }
    return violations;
}

/**
 * @brief Count linear constraint violations via CSC matrix-vector product.
 *
 * Computes activity = A * x using CSC storage, then checks each row:
 *   '<': activity <= rhs + tolerance
 *   '>': activity >= rhs - tolerance
 *   '=': |activity - rhs| <= tolerance
 *
 * @return number of constraint violations, or -1 on allocation failure
 */
static int check_constraints(const CxfModel *model, double tolerance) {
    const MatrixData *mat = model->matrix;
    int m = model->num_constrs;
    int n = model->num_vars;

    if (m == 0 || mat == NULL || mat->col_ptr == NULL) {
        return 0;
    }

    /* Allocate activity vector */
    double *activity = (double *)cxf_calloc((size_t)m, sizeof(double));
    if (activity == NULL) {
        return -1;  /* OOM sentinel */
    }

    /* Compute A * x via CSC traversal */
    for (int j = 0; j < n; j++) {
        double xj = model->solution[j];
        if (xj == 0.0) continue;
        int64_t start = mat->col_ptr[j];
        int64_t end   = mat->col_ptr[j + 1];
        for (int64_t k = start; k < end; k++) {
            int row = mat->row_idx[k];
            activity[row] += mat->values[k] * xj;
        }
    }

    /* Check each constraint against sense and RHS */
    int violations = 0;
    for (int i = 0; i < m; i++) {
        double act = activity[i];
        double rhs = mat->rhs[i];
        char sense = mat->sense[i];

        switch (sense) {
        case '<':
            if (act > rhs + tolerance) violations++;
            break;
        case '>':
            if (act < rhs - tolerance) violations++;
            break;
        case '=':
            if (fabs(act - rhs) > tolerance) violations++;
            break;
        default:
            /* Unknown sense — treat as violation */
            violations++;
            break;
        }
    }

    cxf_free(activity);
    return violations;
}

/**
 * @brief Validate a solution against model bounds and constraints.
 *
 * Per V2 data_validation.md cxf_validate_solution:
 * - Phase 1: Variable bound feasibility
 * - Phase 2: Linear constraint feasibility
 * Reports total violation count via num_violations_out.
 *
 * @param model            Model to validate against
 * @param tolerance        Feasibility tolerance for all checks
 * @param num_violations_out  Output: total violations (may be NULL)
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if model is NULL,
 *         CXF_ERROR_OUT_OF_MEMORY if workspace allocation fails
 */
int cxf_validate_solution(CxfModel *model, double tolerance,
                          int *num_violations_out) {
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int total = 0;

    /* Phase 1: variable bounds */
    if (model->num_vars > 0 && model->solution != NULL) {
        total += check_bounds(model, tolerance);
    }

    /* Phase 2: linear constraints */
    if (model->num_constrs > 0 && model->solution != NULL) {
        int cv = check_constraints(model, tolerance);
        if (cv < 0) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        total += cv;
    }

    if (num_violations_out != NULL) {
        *num_violations_out = total;
    }
    return CXF_OK;
}
