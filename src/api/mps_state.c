/**
 * @file mps_state.c
 * @brief MPS parser state management.
 */

#include <stdlib.h>
#include <string.h>
#include "mps_internal.h"

extern void *cxf_malloc(size_t size);
extern void *cxf_calloc(size_t count, size_t size);
extern void *cxf_realloc(void *ptr, size_t size);
extern void cxf_free(void *ptr);

MpsState *mps_state_create(void) {
    MpsState *s = (MpsState *)cxf_calloc(1, sizeof(MpsState));
    if (!s) return NULL;

    s->rows = (MpsRow *)cxf_malloc(MPS_INITIAL_CAP * sizeof(MpsRow));
    s->cols = (MpsCol *)cxf_malloc(MPS_INITIAL_CAP * sizeof(MpsCol));
    if (!s->rows || !s->cols) {
        cxf_free(s->rows);
        cxf_free(s->cols);
        cxf_free(s);
        return NULL;
    }

    s->row_cap = MPS_INITIAL_CAP;
    s->col_cap = MPS_INITIAL_CAP;
    s->obj_row = -1;
    return s;
}

void mps_state_free(MpsState *state) {
    if (!state) return;
    for (int i = 0; i < state->num_cols; i++) {
        cxf_free(state->cols[i].constr_idx);
        cxf_free(state->cols[i].constr_val);
    }
    cxf_free(state->rows);
    cxf_free(state->cols);
    cxf_free(state);
}

int mps_find_row(MpsState *s, const char *name) {
    for (int i = 0; i < s->num_rows; i++) {
        if (strcmp(s->rows[i].name, name) == 0) return i;
    }
    return -1;
}

int mps_find_col(MpsState *s, const char *name) {
    for (int i = 0; i < s->num_cols; i++) {
        if (strcmp(s->cols[i].name, name) == 0) return i;
    }
    return -1;
}

int mps_add_row(MpsState *s, const char *name, char sense) {
    if (s->num_rows >= s->row_cap) {
        int new_cap = s->row_cap * 2;
        MpsRow *new_rows = cxf_realloc(s->rows, (size_t)new_cap * sizeof(MpsRow));
        if (!new_rows) return -1;
        s->rows = new_rows;
        s->row_cap = new_cap;
    }
    MpsRow *r = &s->rows[s->num_rows];
    strncpy(r->name, name, MPS_MAX_NAME - 1);
    r->name[MPS_MAX_NAME - 1] = '\0';
    r->sense = sense;
    r->rhs = 0.0;
    return s->num_rows++;
}

int mps_add_col(MpsState *s, const char *name) {
    if (s->num_cols >= s->col_cap) {
        int new_cap = s->col_cap * 2;
        MpsCol *new_cols = cxf_realloc(s->cols, (size_t)new_cap * sizeof(MpsCol));
        if (!new_cols) return -1;
        s->cols = new_cols;
        s->col_cap = new_cap;
    }
    MpsCol *c = &s->cols[s->num_cols];
    memset(c, 0, sizeof(MpsCol));
    strncpy(c->name, name, MPS_MAX_NAME - 1);
    c->name[MPS_MAX_NAME - 1] = '\0';
    c->lb = 0.0;
    c->ub = CXF_INFINITY;
    return s->num_cols++;
}

int mps_add_coeff(MpsState *s, int col_idx, int row_idx, double val) {
    MpsCol *c = &s->cols[col_idx];
    if (c->ncoeffs >= c->coeff_cap) {
        int new_cap = (c->coeff_cap == 0) ? 8 : c->coeff_cap * 2;
        int *new_idx = cxf_realloc(c->constr_idx, (size_t)new_cap * sizeof(int));
        double *new_val = cxf_realloc(c->constr_val, (size_t)new_cap * sizeof(double));
        if (!new_idx || !new_val) {
            cxf_free(new_idx);
            return -1;
        }
        c->constr_idx = new_idx;
        c->constr_val = new_val;
        c->coeff_cap = new_cap;
    }
    c->constr_idx[c->ncoeffs] = row_idx;
    c->constr_val[c->ncoeffs] = val;
    c->ncoeffs++;
    return 0;
}
