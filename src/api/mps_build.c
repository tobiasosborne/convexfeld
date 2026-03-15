/**
 * @file mps_build.c
 * @brief Build CxfModel from parsed MPS data.
 *
 * Performance optimization: Build CSC matrix directly in O(nnz) instead of
 * calling cxf_addconstr() per row which is O(nnz^2) due to array shifting.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mps_internal.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"

#include "../memory/memory_internal.h"
#include "../matrix/matrix_internal.h"


/**
 * @brief Build CSC matrix directly from MPS column data.
 *
 * Two-pass O(nnz) algorithm:
 * 1. Count entries per column to build col_ptr
 * 2. Fill row_idx and values in column order
 *
 * This replaces the O(nnz^2) approach of calling cxf_addconstr() per row.
 */
static int build_csc_direct(MpsState *s, CxfModel *model, const int *row_map,
                            int num_constrs) {
    MatrixData *mat = model->matrix;
    int num_cols = s->num_cols;
    int64_t total_nnz = 0;

    /* Pass 1: Count entries per column and total nnz */
    int64_t *col_counts = (int64_t *)cxf_calloc((size_t)(num_cols + 1),
                                                  sizeof(int64_t));
    if (!col_counts) return CXF_ERROR_OUT_OF_MEMORY;

    for (int col = 0; col < num_cols; col++) {
        MpsCol *c = &s->cols[col];
        for (int k = 0; k < c->ncoeffs; k++) {
            int mps_row = c->constr_idx[k];
            int mapped = row_map[mps_row];
            if (mapped >= 0) {
                /* Reject NaN/Inf coefficients (numerical_stability.md §C) */
                if (!isfinite(c->constr_val[k])) {
                    cxf_free(col_counts);
                    return CXF_ERROR_INVALID_ARGUMENT;
                }
                col_counts[col]++;
                total_nnz++;
            }
        }
    }

    /* Initialize CSC arrays */
    int status = cxf_sparse_init_csc(mat, num_constrs, num_cols, total_nnz);
    if (status != CXF_OK) {
        cxf_free(col_counts);
        return status;
    }

    /* Build col_ptr from counts (cumulative sum) */
    mat->col_ptr[0] = 0;
    for (int col = 0; col < num_cols; col++) {
        mat->col_ptr[col + 1] = mat->col_ptr[col] + col_counts[col];
    }

    /* Pass 2: Fill row_idx and values */
    /* Reset col_counts to track insertion positions */
    memset(col_counts, 0, (size_t)(num_cols + 1) * sizeof(int64_t));

    for (int col = 0; col < num_cols; col++) {
        MpsCol *c = &s->cols[col];
        for (int k = 0; k < c->ncoeffs; k++) {
            int mps_row = c->constr_idx[k];
            int mapped = row_map[mps_row];
            if (mapped >= 0) {
                int64_t pos = mat->col_ptr[col] + col_counts[col];
                mat->row_idx[pos] = mapped;
                mat->values[pos] = c->constr_val[k];
                col_counts[col]++;
            }
        }
    }

    cxf_free(col_counts);

    /* Allocate and fill rhs and sense arrays */
    mat->rhs = (double *)cxf_malloc((size_t)num_constrs * sizeof(double));
    mat->sense = (char *)cxf_malloc((size_t)num_constrs * sizeof(char));
    if (!mat->rhs || !mat->sense) {
        cxf_free(mat->rhs);
        cxf_free(mat->sense);
        mat->rhs = NULL;
        mat->sense = NULL;
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Check if any row has a range value */
    int has_ranges = 0;
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].range_value != 0.0) { has_ranges = 1; break; }
    }
    if (has_ranges) {
        mat->range_values = (double *)cxf_calloc((size_t)num_constrs,
                                                  sizeof(double));
    }

    int constr_idx = 0;
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense == 'N') continue;
        /* Reject NaN/Inf RHS (numerical_stability.md §C) */
        if (!isfinite(s->rows[i].rhs)) return CXF_ERROR_INVALID_ARGUMENT;
        mat->rhs[constr_idx] = s->rows[i].rhs;
        mat->sense[constr_idx] = s->rows[i].sense;
        if (mat->range_values)
            mat->range_values[constr_idx] = s->rows[i].range_value;
        constr_idx++;
    }

    model->num_constrs = num_constrs;
    return CXF_OK;
}

int mps_build_model(MpsState *s, CxfModel *model) {
    int status;
    int *row_map = NULL;
    int num_constrs = 0;

    /* Count constraints (non-objective rows) */
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense != 'N') num_constrs++;
    }

    /* Create row mapping */
    row_map = (int *)cxf_malloc((size_t)s->num_rows * sizeof(int));
    if (!row_map) return CXF_ERROR_OUT_OF_MEMORY;

    int constr_idx = 0;
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense == 'N') {
            row_map[i] = -1;
        } else {
            row_map[i] = constr_idx++;
        }
    }

    /* Add variables (without constraint coefficients) */
    for (int col = 0; col < s->num_cols; col++) {
        MpsCol *c = &s->cols[col];
        status = cxf_addvar(model, 0, NULL, NULL,
                           c->obj_coeff, c->lb, c->ub, CXF_CONTINUOUS, c->name);
        if (status != CXF_OK) {
            cxf_free(row_map);
            return status;
        }
    }

    /* Build CSC matrix directly - O(nnz) instead of O(nnz^2) */
    status = build_csc_direct(s, model, row_map, num_constrs);
    cxf_free(row_map);
    return status;
}
