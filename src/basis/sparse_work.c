/**
 * @file sparse_work.c
 * @brief Sparse working matrix for LU factorization.
 *
 * Column-oriented storage for Markowitz-ordered sparse Gaussian
 * elimination. Suhl & Suhl (1990), Maros (2003) Chapter 5.
 *
 * Spec: product_form_inverse.md Step 1, numerical_stability.md §D
 */

#include "basis_internal.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

#define INITIAL_COL_CAP 8
#define MIN_PIVOT       CXF_MIN_PIVOT  /* 1e-13 */

int sparse_col_append(SparseCol *col, int row, double val) {
    if (col->len >= col->cap) {
        int new_cap = col->cap > 0 ? col->cap * 2 : INITIAL_COL_CAP;
        int *nr = realloc(col->row_idx, (size_t)new_cap * sizeof(int));
        double *nv = realloc(col->values, (size_t)new_cap * sizeof(double));
        if (!nr || !nv) { free(nr); free(nv); return 1001; }
        col->row_idx = nr;
        col->values = nv;
        col->cap = new_cap;
    }
    col->row_idx[col->len] = row;
    col->values[col->len] = val;
    col->len++;
    return 0;
}

SparseWork *sparse_work_create(int m) {
    if (m <= 0) return NULL;
    SparseWork *sw = calloc(1, sizeof(SparseWork));
    if (!sw) return NULL;

    sw->m = m;
    sw->cols = calloc((size_t)m, sizeof(SparseCol));
    sw->row_count = calloc((size_t)m, sizeof(int));
    sw->col_max = calloc((size_t)m, sizeof(double));
    sw->row_active = calloc((size_t)m, sizeof(int));
    sw->col_active = calloc((size_t)m, sizeof(int));

    if (!sw->cols || !sw->row_count || !sw->col_max ||
        !sw->row_active || !sw->col_active) {
        sparse_work_free(sw);
        return NULL;
    }

    for (int i = 0; i < m; i++) {
        sw->row_active[i] = 1;
        sw->col_active[i] = 1;
    }
    sw->active_count = m;
    sw->total_nnz = 0;
    return sw;
}

void sparse_work_free(SparseWork *sw) {
    if (!sw) return;
    if (sw->cols) {
        for (int j = 0; j < sw->m; j++) {
            free(sw->cols[j].row_idx);
            free(sw->cols[j].values);
        }
    }
    free(sw->cols);
    free(sw->row_count);
    free(sw->col_max);
    free(sw->row_active);
    free(sw->col_active);
    free(sw);
}

double sparse_work_density(const SparseWork *sw) {
    if (!sw || sw->active_count <= 0) return 0.0;
    return (double)sw->total_nnz /
           ((double)sw->active_count * (double)sw->active_count);
}

int sparse_work_extract(SparseWork *sw, SolverState *ctx) {
    if (!sw || !ctx || !ctx->basis) return CXF_ERROR_NULL_ARGUMENT;

    BasisState *basis = ctx->basis;
    int m = basis->m;
    int n_orig = ctx->num_vars;

    for (int j = 0; j < m; j++) {
        int var = basis->basic_vars[j];
        if (var < n_orig && ctx->csc_col_ptr != NULL) {
            /* Structural variable: read from CSC matrix */
            for (int64_t k = ctx->csc_col_ptr[var];
                 k < ctx->csc_col_ptr[var + 1]; k++) {
                int row = ctx->csc_row_idx[k];
                double val = ctx->csc_values[k];
                if (row < m && fabs(val) >= MIN_PIVOT) {
                    int rc = sparse_col_append(&sw->cols[j], row, val);
                    if (rc) return rc;
                }
            }
        } else if (var >= n_orig && var < n_orig + m) {
            /* Slack/surplus: single entry from diag_coeff */
            int slack_row = var - n_orig;
            if (slack_row >= 0 && slack_row < m) {
                double val = basis->diag_coeff[slack_row];
                if (fabs(val) >= MIN_PIVOT) {
                    int rc = sparse_col_append(&sw->cols[j], slack_row, val);
                    if (rc) return rc;
                }
            }
        }
    }

    /* Compute row_count and col_max from populated columns */
    for (int j = 0; j < m; j++) {
        SparseCol *col = &sw->cols[j];
        double cmax = 0.0;
        for (int k = 0; k < col->len; k++) {
            sw->row_count[col->row_idx[k]]++;
            double av = fabs(col->values[k]);
            if (av > cmax) cmax = av;
        }
        sw->col_max[j] = cmax;
        sw->total_nnz += col->len;
    }
    return 0;
}
