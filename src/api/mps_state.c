/**
 * @file mps_state.c
 * @brief MPS parser state management with hash table lookups.
 */

#include <stdlib.h>
#include <string.h>
#include "mps_internal.h"

extern void *cxf_malloc(size_t size);
extern void *cxf_calloc(size_t count, size_t size);
extern void *cxf_realloc(void *ptr, size_t size);
extern void cxf_free(void *ptr);

/**
 * @brief djb2 hash function for strings.
 * Simple and fast hash with good distribution.
 */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    }
    return hash & (MPS_HASH_SIZE - 1);  /* Fast modulo for power-of-2 size */
}

/**
 * @brief Free all entries in a hash table.
 */
static void hash_table_free(MpsHashEntry **table) {
    for (int i = 0; i < MPS_HASH_SIZE; i++) {
        MpsHashEntry *entry = table[i];
        while (entry) {
            MpsHashEntry *next = entry->next;
            cxf_free(entry);
            entry = next;
        }
    }
}

/**
 * @brief Look up a name in a hash table.
 * @return Index if found, -1 if not found.
 */
static int hash_table_find(MpsHashEntry **table, const char *name) {
    unsigned int bucket = hash_string(name);
    MpsHashEntry *entry = table[bucket];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry->index;
        }
        entry = entry->next;
    }
    return -1;
}

/**
 * @brief Add a name to a hash table.
 * @return 0 on success, -1 on allocation failure.
 */
static int hash_table_add(MpsHashEntry **table, const char *name, int index) {
    unsigned int bucket = hash_string(name);
    MpsHashEntry *entry = (MpsHashEntry *)cxf_malloc(sizeof(MpsHashEntry));
    if (!entry) return -1;

    strncpy(entry->name, name, MPS_MAX_NAME - 1);
    entry->name[MPS_MAX_NAME - 1] = '\0';
    entry->index = index;
    entry->next = table[bucket];
    table[bucket] = entry;
    return 0;
}

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
    /* Hash tables are zeroed by calloc */
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
    /* Free hash table entries */
    hash_table_free(state->row_hash);
    hash_table_free(state->col_hash);
    cxf_free(state);
}

int mps_find_row(MpsState *s, const char *name) {
    return hash_table_find(s->row_hash, name);
}

int mps_find_col(MpsState *s, const char *name) {
    return hash_table_find(s->col_hash, name);
}

int mps_add_row(MpsState *s, const char *name, char sense) {
    if (s->num_rows >= s->row_cap) {
        int new_cap = s->row_cap * 2;
        MpsRow *new_rows = cxf_realloc(s->rows, (size_t)new_cap * sizeof(MpsRow));
        if (!new_rows) return -1;
        s->rows = new_rows;
        s->row_cap = new_cap;
    }
    int idx = s->num_rows;
    MpsRow *r = &s->rows[idx];
    strncpy(r->name, name, MPS_MAX_NAME - 1);
    r->name[MPS_MAX_NAME - 1] = '\0';
    r->sense = sense;
    r->rhs = 0.0;

    /* Add to hash table */
    if (hash_table_add(s->row_hash, name, idx) < 0) {
        return -1;
    }

    s->num_rows++;
    return idx;
}

int mps_add_col(MpsState *s, const char *name) {
    if (s->num_cols >= s->col_cap) {
        int new_cap = s->col_cap * 2;
        MpsCol *new_cols = cxf_realloc(s->cols, (size_t)new_cap * sizeof(MpsCol));
        if (!new_cols) return -1;
        s->cols = new_cols;
        s->col_cap = new_cap;
    }
    int idx = s->num_cols;
    MpsCol *c = &s->cols[idx];
    memset(c, 0, sizeof(MpsCol));
    strncpy(c->name, name, MPS_MAX_NAME - 1);
    c->name[MPS_MAX_NAME - 1] = '\0';
    c->lb = 0.0;
    c->ub = CXF_INFINITY;

    /* Add to hash table */
    if (hash_table_add(s->col_hash, name, idx) < 0) {
        return -1;
    }

    s->num_cols++;
    return idx;
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
