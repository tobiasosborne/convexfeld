/**
 * @file presolve.c
 * @brief Presolve detection and unconstrained solver.
 *
 * Extracted from solve_lp.c. Contains obvious infeasibility/unboundedness
 * checks via bound propagation and ray analysis, plus unconstrained solver.
 */

#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern int cxf_prepare_row_data(MatrixData *mat);
extern int cxf_build_row_major(MatrixData *mat);

/** @brief Get row coefficients as dense array (CSR fast path, CSC fallback). */
static void get_row_coeffs(MatrixData *mat, int row, int n, double *coeffs) {
    memset(coeffs, 0, (size_t)n * sizeof(double));
    if (mat->row_ptr != NULL && mat->col_idx != NULL && mat->row_values != NULL) {
        int64_t start = mat->row_ptr[row];
        int64_t end = mat->row_ptr[row + 1];
        for (int64_t k = start; k < end; k++) {
            int col = mat->col_idx[k];
            if (col >= 0 && col < n) coeffs[col] = mat->row_values[k];
        }
        return;
    }
    for (int j = 0; j < n; j++) {
        int64_t start = mat->col_ptr[j];
        int64_t end = mat->col_ptr[j + 1];
        for (int64_t k = start; k < end; k++) {
            if (mat->row_idx[k] == row) { coeffs[j] = mat->values[k]; break; }
        }
    }
}

/** @brief Check if two rows are parallel (same direction). */
static int rows_parallel(double *r1, double *r2, int n, double *scale) {
    double s = 0.0;
    int found = 0;
    for (int j = 0; j < n; j++) {
        if (fabs(r1[j]) < CXF_ZERO_TOL && fabs(r2[j]) < CXF_ZERO_TOL) continue;
        if (fabs(r1[j]) < CXF_ZERO_TOL || fabs(r2[j]) < CXF_ZERO_TOL) return 0;
        double ratio = r1[j] / r2[j];
        if (!found) { s = ratio; found = 1; }
        else if (fabs(ratio - s) > CXF_FEASIBILITY_TOL) return 0;
    }
    *scale = s;
    return found;
}

#define MAX_PARALLEL_CHECK_ROWS 100

int cxf_check_obvious_infeasibility(CxfModel *model) {
    MatrixData *mat = model->matrix;
    if (mat == NULL) return 0;
    int m = mat->num_rows;
    int n = mat->num_cols;

    if (mat->row_ptr == NULL) {
        if (cxf_prepare_row_data(mat) == CXF_OK)
            cxf_build_row_major(mat);
    }

    double *row1 = (double *)malloc((size_t)n * sizeof(double));
    double *row2 = (double *)malloc((size_t)n * sizeof(double));
    if (row1 == NULL || row2 == NULL) { free(row1); free(row2); return 0; }

    /* Check 1: Single constraint infeasibility via bound propagation */
    for (int i = 0; i < m; i++) {
        double row_min = 0.0, row_max = 0.0;
        get_row_coeffs(mat, i, n, row1);
        for (int j = 0; j < n; j++) {
            double aij = row1[j];
            if (aij == 0.0) continue;
            double lb = model->lb[j], ub = model->ub[j];
            if (aij > 0) {
                row_min += aij * lb;
                row_max += (ub >= CXF_INFINITY) ? CXF_INFINITY : aij * ub;
            } else {
                row_min += (ub >= CXF_INFINITY) ? -CXF_INFINITY : aij * ub;
                row_max += aij * lb;
            }
        }
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        char sense = mat->sense ? mat->sense[i] : '<';
        if ((sense == '<' || sense == 'L') && row_min > rhs + CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if ((sense == '>' || sense == 'G') && row_max < rhs - CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if (sense == '=') {
            if (row_min > rhs + CXF_FEASIBILITY_TOL || row_max < rhs - CXF_FEASIBILITY_TOL) {
                free(row1); free(row2); return 1;
            }
        }
    }

    /* Check 2: Parallel constraint contradiction (small problems only) */
    if (m <= MAX_PARALLEL_CHECK_ROWS) {
        for (int i = 0; i < m; i++) {
            get_row_coeffs(mat, i, n, row1);
            double rhs1 = mat->rhs ? mat->rhs[i] : 0.0;
            char sense1 = mat->sense ? mat->sense[i] : '<';
            for (int j = i + 1; j < m; j++) {
                get_row_coeffs(mat, j, n, row2);
                double sc = 0.0;
                if (!rows_parallel(row1, row2, n, &sc)) continue;
                double rhs2 = mat->rhs ? mat->rhs[j] : 0.0;
                char sense2 = mat->sense ? mat->sense[j] : '<';
                double scaled_rhs2 = rhs2 * sc;
                char scaled_sense2 = sense2;
                if (sc < 0) {
                    if (sense2 == '<') scaled_sense2 = '>';
                    else if (sense2 == '>') scaled_sense2 = '<';
                }
                double lower = -CXF_INFINITY, upper = CXF_INFINITY;
                if (sense1 == '<' || sense1 == 'L') upper = fmin(upper, rhs1);
                else if (sense1 == '>' || sense1 == 'G') lower = fmax(lower, rhs1);
                else { lower = rhs1; upper = rhs1; }
                if (scaled_sense2 == '<' || scaled_sense2 == 'L') upper = fmin(upper, scaled_rhs2);
                else if (scaled_sense2 == '>' || scaled_sense2 == 'G') lower = fmax(lower, scaled_rhs2);
                else { lower = fmax(lower, scaled_rhs2); upper = fmin(upper, scaled_rhs2); }
                if (lower > upper + CXF_FEASIBILITY_TOL) {
                    free(row1); free(row2); return 1;
                }
            }
        }
    }

    free(row1); free(row2);
    return 0;
}

int cxf_check_obvious_unboundedness(CxfModel *model) {
    MatrixData *mat = model->matrix;
    if (mat == NULL) return 0;
    int m = mat->num_rows;
    int n = mat->num_cols;

    for (int j = 0; j < n; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];

        if (c < -CXF_FEASIBILITY_TOL && ub >= CXF_INFINITY) {
            int can_increase = 1;
            for (int i = 0; i < m && can_increase; i++) {
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;
                char sense = mat->sense ? mat->sense[i] : '<';
                if ((sense == '<' || sense == 'L') && aij > CXF_ZERO_TOL) can_increase = 0;
                if ((sense == '>' || sense == 'G') && aij < -CXF_ZERO_TOL) can_increase = 0;
                if (sense == '=') can_increase = 0;
            }
            if (can_increase) return 1;
        }

        if (c > CXF_FEASIBILITY_TOL && lb <= -CXF_INFINITY) {
            int can_decrease = 1;
            for (int i = 0; i < m && can_decrease; i++) {
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;
                char sense = mat->sense ? mat->sense[i] : '<';
                if ((sense == '<' || sense == 'L') && aij < -CXF_ZERO_TOL) can_decrease = 0;
                if ((sense == '>' || sense == 'G') && aij > CXF_ZERO_TOL) can_decrease = 0;
                if (sense == '=') can_decrease = 0;
            }
            if (can_decrease) return 1;
        }
    }
    return 0;
}

int cxf_solve_unconstrained(CxfModel *model) {
    for (int j = 0; j < model->num_vars; j++) {
        if (model->lb[j] > model->ub[j] + CXF_FEASIBILITY_TOL) {
            model->status = CXF_INFEASIBLE;
            return CXF_INFEASIBLE;
        }
    }
    double obj_val = 0.0;
    for (int j = 0; j < model->num_vars; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];
        if (c < 0) {
            if (ub >= CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = ub;
            obj_val += c * ub;
        } else if (c > 0) {
            if (lb <= -CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = lb;
            obj_val += c * lb;
        } else {
            double val = (lb > 0.0) ? lb : ((ub < 0.0) ? ub : 0.0);
            if (model->solution) model->solution[j] = val;
        }
    }
    model->obj_val = obj_val;
    model->status = CXF_OPTIMAL;
    return CXF_OPTIMAL;
}
