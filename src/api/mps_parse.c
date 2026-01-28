/**
 * @file mps_parse.c
 * @brief MPS file parsing logic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mps_internal.h"

/* Parser sections */
typedef enum {
    SEC_NONE, SEC_NAME, SEC_ROWS, SEC_COLUMNS, SEC_RHS, SEC_BOUNDS, SEC_RANGES
} MpsSection;

/* Skip leading whitespace */
static char *skip_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Read next token from line */
static int next_token(const char *line, int *pos, char *buf, int buflen) {
    int i = *pos;
    int j = 0;
    while (line[i] && isspace((unsigned char)line[i])) i++;
    if (!line[i]) { buf[0] = '\0'; return 0; }
    while (line[i] && !isspace((unsigned char)line[i]) && j < buflen - 1) {
        buf[j++] = line[i++];
    }
    buf[j] = '\0';
    *pos = i;
    return j > 0;
}

/* Get section from header line */
static MpsSection get_section(const char *line) {
    if (strncmp(line, "NAME", 4) == 0) return SEC_NAME;
    if (strncmp(line, "ROWS", 4) == 0) return SEC_ROWS;
    if (strncmp(line, "COLUMNS", 7) == 0) return SEC_COLUMNS;
    if (strncmp(line, "RHS", 3) == 0) return SEC_RHS;
    if (strncmp(line, "BOUNDS", 6) == 0) return SEC_BOUNDS;
    if (strncmp(line, "RANGES", 6) == 0) return SEC_RANGES;
    return SEC_NONE;
}

/* Parse ROWS section line */
static int parse_row_line(MpsState *s, const char *line) {
    int pos = 0;
    char tok[MPS_MAX_NAME], name[MPS_MAX_NAME];

    if (!next_token(line, &pos, tok, sizeof(tok))) return CXF_OK;
    if (!next_token(line, &pos, name, sizeof(name))) return CXF_ERROR_INVALID_ARGUMENT;

    char sense;
    switch (tok[0]) {
        case 'N': sense = 'N'; break;
        case 'E': sense = '='; break;
        case 'L': sense = '<'; break;
        case 'G': sense = '>'; break;
        default: return CXF_ERROR_INVALID_ARGUMENT;
    }

    int idx = mps_add_row(s, name, sense);
    if (idx < 0) return CXF_ERROR_OUT_OF_MEMORY;

    if (sense == 'N' && s->obj_row < 0) s->obj_row = idx;
    return CXF_OK;
}

/* Parse COLUMNS section line */
static int parse_col_line(MpsState *s, const char *line) {
    int pos = 0;
    char col_name[MPS_MAX_NAME], row_name[MPS_MAX_NAME], val_str[32];

    if (!next_token(line, &pos, col_name, sizeof(col_name))) return CXF_OK;

    int col_idx = mps_find_col(s, col_name);
    if (col_idx < 0) {
        col_idx = mps_add_col(s, col_name);
        if (col_idx < 0) return CXF_ERROR_OUT_OF_MEMORY;
    }

    while (next_token(line, &pos, row_name, sizeof(row_name))) {
        if (!next_token(line, &pos, val_str, sizeof(val_str))) break;
        double val = atof(val_str);

        int row_idx = mps_find_row(s, row_name);
        if (row_idx < 0) continue;

        if (s->rows[row_idx].sense == 'N') {
            s->cols[col_idx].obj_coeff = val;
        } else {
            if (mps_add_coeff(s, col_idx, row_idx, val) < 0)
                return CXF_ERROR_OUT_OF_MEMORY;
        }
    }
    return CXF_OK;
}

/* Parse RHS section line */
static int parse_rhs_line(MpsState *s, const char *line) {
    int pos = 0;
    char tok[MPS_MAX_NAME], row_name[MPS_MAX_NAME], val_str[32];

    if (!next_token(line, &pos, tok, sizeof(tok))) return CXF_OK;

    while (next_token(line, &pos, row_name, sizeof(row_name))) {
        if (!next_token(line, &pos, val_str, sizeof(val_str))) break;
        double val = atof(val_str);

        int row_idx = mps_find_row(s, row_name);
        if (row_idx >= 0) s->rows[row_idx].rhs = val;
    }
    return CXF_OK;
}

/* Parse BOUNDS section line */
static int parse_bounds_line(MpsState *s, const char *line) {
    int pos = 0;
    char type[4], bnd_name[MPS_MAX_NAME], col_name[MPS_MAX_NAME], val_str[32];

    if (!next_token(line, &pos, type, sizeof(type))) return CXF_OK;
    if (!next_token(line, &pos, bnd_name, sizeof(bnd_name))) return CXF_OK;
    if (!next_token(line, &pos, col_name, sizeof(col_name))) return CXF_OK;

    int col_idx = mps_find_col(s, col_name);
    if (col_idx < 0) return CXF_OK;

    double val = 0.0;
    if (next_token(line, &pos, val_str, sizeof(val_str))) val = atof(val_str);

    MpsCol *c = &s->cols[col_idx];
    if (strcmp(type, "LO") == 0) c->lb = val;
    else if (strcmp(type, "UP") == 0) c->ub = val;
    else if (strcmp(type, "FX") == 0) { c->lb = val; c->ub = val; }
    else if (strcmp(type, "FR") == 0) { c->lb = -CXF_INFINITY; c->ub = CXF_INFINITY; }
    else if (strcmp(type, "MI") == 0) c->lb = -CXF_INFINITY;
    else if (strcmp(type, "PL") == 0) c->ub = CXF_INFINITY;
    else if (strcmp(type, "BV") == 0) { c->lb = 0.0; c->ub = 1.0; }

    return CXF_OK;
}

int mps_parse_file(MpsState *s, FILE *fp) {
    char line[MPS_MAX_LINE];
    MpsSection section = SEC_NONE;
    int status = CXF_OK;

    while (fgets(line, sizeof(line), fp)) {
        char *p = skip_ws(line);
        if (!*p || *p == '*') continue;

        if (!isspace((unsigned char)line[0])) {
            section = get_section(p);
            if (section == SEC_NAME) {
                int pos = 4;
                char tok[MPS_MAX_NAME];
                next_token(p, &pos, tok, sizeof(tok));
                strncpy(s->name, tok, MPS_MAX_NAME - 1);
                s->name[MPS_MAX_NAME - 1] = '\0';
            }
            continue;
        }

        switch (section) {
            case SEC_ROWS: status = parse_row_line(s, p); break;
            case SEC_COLUMNS: status = parse_col_line(s, p); break;
            case SEC_RHS: status = parse_rhs_line(s, p); break;
            case SEC_BOUNDS: status = parse_bounds_line(s, p); break;
            default: break;
        }
        if (status != CXF_OK) return status;
    }
    return CXF_OK;
}
