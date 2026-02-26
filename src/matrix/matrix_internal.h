/**
 * @file matrix_internal.h
 * @brief Internal declarations for matrix module functions.
 */

#ifndef MATRIX_INTERNAL_H
#define MATRIX_INTERNAL_H

#include <stdint.h>

struct MatrixData;

struct MatrixData *cxf_sparse_create(void);
void cxf_sparse_free(struct MatrixData *mat);
int cxf_sparse_init_csc(struct MatrixData *mat, int num_rows, int num_cols,
                        int64_t nnz);
int cxf_prepare_row_data(struct MatrixData *mat);
int cxf_build_row_major(struct MatrixData *mat);

#endif /* MATRIX_INTERNAL_H */
