/**
 * @file mps_parser.c
 * @brief MPS file parser - main entry point.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_mps.h"
#include "mps_internal.h"

extern int cxf_checkmodel(CxfModel *model);

int cxf_readmps(CxfModel *model, const char *filename) {
    FILE *fp = NULL;
    MpsState *state = NULL;
    int status = CXF_OK;

    /* Validate inputs */
    status = cxf_checkmodel(model);
    if (status != CXF_OK) return status;

    if (filename == NULL || strlen(filename) == 0) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Open file */
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Create parser state */
    state = mps_state_create();
    if (state == NULL) {
        fclose(fp);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Parse file */
    status = mps_parse_file(state, fp);
    fclose(fp);

    if (status != CXF_OK) {
        mps_state_free(state);
        return status;
    }

    /* Build model from parsed data */
    status = mps_build_model(state, model);
    mps_state_free(state);

    return status;
}
