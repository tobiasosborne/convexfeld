/**
 * @file mps_internal.h
 * @brief Internal MPS parser data structures.
 */

#ifndef MPS_INTERNAL_H
#define MPS_INTERNAL_H

#include <stdio.h>
#include "convexfeld/cxf_types.h"

#define MPS_MAX_NAME 16
#define MPS_MAX_LINE 256
#define MPS_INITIAL_CAP 64

/* Row (constraint/objective) entry */
typedef struct {
    char name[MPS_MAX_NAME];
    char sense;  /* N=obj, '='/'<'/'>' for constraints */
    double rhs;
} MpsRow;

/* Column (variable) entry */
typedef struct {
    char name[MPS_MAX_NAME];
    double obj_coeff;
    double lb;
    double ub;
    int *constr_idx;   /* Constraint indices for coefficients */
    double *constr_val; /* Coefficient values */
    int ncoeffs;
    int coeff_cap;
} MpsCol;

/* Parser state */
typedef struct MpsState {
    char name[MPS_MAX_NAME];
    MpsRow *rows;
    int num_rows;
    int row_cap;
    MpsCol *cols;
    int num_cols;
    int col_cap;
    int obj_row;  /* Index of objective row (-1 if not found) */
} MpsState;

/* State management */
MpsState *mps_state_create(void);
void mps_state_free(MpsState *state);

/* Lookup functions */
int mps_find_row(MpsState *s, const char *name);
int mps_find_col(MpsState *s, const char *name);

/* Add functions */
int mps_add_row(MpsState *s, const char *name, char sense);
int mps_add_col(MpsState *s, const char *name);
int mps_add_coeff(MpsState *s, int col_idx, int row_idx, double val);

/* Parsing */
int mps_parse_file(MpsState *state, FILE *fp);

/* Model building */
int mps_build_model(MpsState *state, CxfModel *model);

#endif /* MPS_INTERNAL_H */
