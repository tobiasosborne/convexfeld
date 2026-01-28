/**
 * @file mps_build.c
 * @brief Build CxfModel from parsed MPS data.
 */

#include <stdlib.h>
#include <string.h>
#include "mps_internal.h"
#include "convexfeld/cxf_model.h"

extern void *cxf_malloc(size_t size);
extern void *cxf_calloc(size_t count, size_t size);
extern void *cxf_realloc(void *ptr, size_t size);
extern void cxf_free(void *ptr);
extern int cxf_addvar(CxfModel *model, int numnz, int *vind, double *vval,
                      double obj, double lb, double ub, char vtype, const char *name);
extern int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                         const double *cval, char sense, double rhs, const char *name);

/* Build row->column coefficient mapping */
typedef struct {
    int *col_idx;
    double *col_val;
    int nz;
    int cap;
} RowCoeffs;

static int add_row_coeff(RowCoeffs *r, int col, double val) {
    if (r->nz >= r->cap) {
        int new_cap = (r->cap == 0) ? 8 : r->cap * 2;
        int *new_idx = cxf_realloc(r->col_idx, (size_t)new_cap * sizeof(int));
        double *new_val = cxf_realloc(r->col_val, (size_t)new_cap * sizeof(double));
        if (!new_idx || !new_val) return -1;
        r->col_idx = new_idx;
        r->col_val = new_val;
        r->cap = new_cap;
    }
    r->col_idx[r->nz] = col;
    r->col_val[r->nz] = val;
    r->nz++;
    return 0;
}

int mps_build_model(MpsState *s, CxfModel *model) {
    int status;
    int *row_map = NULL;
    RowCoeffs *row_coeffs = NULL;
    int num_constrs = 0;

    /* Count constraints (non-objective rows) */
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense != 'N') num_constrs++;
    }

    /* Create row mapping and coefficient storage */
    row_map = (int *)cxf_malloc((size_t)s->num_rows * sizeof(int));
    row_coeffs = (RowCoeffs *)cxf_calloc((size_t)num_constrs, sizeof(RowCoeffs));
    if (!row_map || !row_coeffs) {
        cxf_free(row_map);
        cxf_free(row_coeffs);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Build row mapping */
    int constr_idx = 0;
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense == 'N') {
            row_map[i] = -1;
        } else {
            row_map[i] = constr_idx++;
        }
    }

    /* Add variables first (without constraint coefficients) */
    for (int col = 0; col < s->num_cols; col++) {
        MpsCol *c = &s->cols[col];
        status = cxf_addvar(model, 0, NULL, NULL,
                           c->obj_coeff, c->lb, c->ub, CXF_CONTINUOUS, c->name);
        if (status != CXF_OK) goto cleanup;

        /* Transpose: for each coefficient in this column, add to row storage */
        for (int k = 0; k < c->ncoeffs; k++) {
            int mps_row = c->constr_idx[k];
            int mapped = row_map[mps_row];
            if (mapped >= 0) {
                if (add_row_coeff(&row_coeffs[mapped], col, c->constr_val[k]) < 0) {
                    status = CXF_ERROR_OUT_OF_MEMORY;
                    goto cleanup;
                }
            }
        }
    }

    /* Add constraints with transposed coefficients */
    constr_idx = 0;
    for (int i = 0; i < s->num_rows; i++) {
        if (s->rows[i].sense == 'N') continue;

        MpsRow *r = &s->rows[i];
        RowCoeffs *rc = &row_coeffs[constr_idx];

        status = cxf_addconstr(model, rc->nz, rc->col_idx, rc->col_val,
                              r->sense, r->rhs, r->name);
        if (status != CXF_OK) goto cleanup;
        constr_idx++;
    }

    status = CXF_OK;

cleanup:
    if (row_coeffs) {
        for (int i = 0; i < num_constrs; i++) {
            cxf_free(row_coeffs[i].col_idx);
            cxf_free(row_coeffs[i].col_val);
        }
        cxf_free(row_coeffs);
    }
    cxf_free(row_map);
    return status;
}
