/**
 * @file lu_output.c
 * @brief CSC assembly from COO arrays for LU factorization output.
 *
 * Converts the COO (coordinate) format L and U entries produced during
 * LU factorization into CSC (compressed sparse column) format stored
 * in the LUFactors structure.
 *
 * Spec: product_form_inverse.md Step 1
 */

#include "basis_internal.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

int build_lu_output(LUFactors *lu, int m,
                    const int *Li, const int *Lj, const double *Lv,
                    int Lc,
                    const int *Ui, const int *Uj, const double *Uv,
                    int Uc) {
    /* Build inverse permutations */
    int *inv_row = malloc((size_t)m * sizeof(int));
    int *inv_col = malloc((size_t)m * sizeof(int));
    if (!inv_row || !inv_col) { free(inv_row); free(inv_col); return 1001; }
    for (int k = 0; k < m; k++) {
        inv_row[lu->perm_row[k]] = k;
        inv_col[lu->perm_col[k]] = k;
    }

    /* --- Build L in CSC --- */
    lu->L_nnz = (int64_t)Lc;
    memset(lu->L_col_ptr, 0, (size_t)(m + 1) * sizeof(int64_t));
    if (Lc > 0) {
        int *lr = realloc(lu->L_row_idx, (size_t)Lc * sizeof(int));
        double *lv = realloc(lu->L_values, (size_t)Lc * sizeof(double));
        if (!lr || !lv) { free(inv_row); free(inv_col); return 1001; }
        lu->L_row_idx = lr; lu->L_values = lv;
    }
    for (int k = 0; k < Lc; k++) lu->L_col_ptr[Lj[k] + 1]++;
    for (int j = 1; j <= m; j++) lu->L_col_ptr[j] += lu->L_col_ptr[j - 1];
    int64_t *wp = calloc((size_t)m, sizeof(int64_t));
    if (!wp) { free(inv_row); free(inv_col); return 1001; }
    for (int k = 0; k < Lc; k++) {
        int col = Lj[k];
        int64_t pos = lu->L_col_ptr[col] + wp[col];
        lu->L_row_idx[pos] = inv_row[Li[k]];  /* Convert to step space */
        lu->L_values[pos] = Lv[k];
        wp[col]++;
    }
    free(wp);

    /* --- Build U in CSC (indexed by step-row, entries are step-cols) --- */
    lu->U_nnz = (int64_t)Uc;
    memset(lu->U_col_ptr, 0, (size_t)(m + 1) * sizeof(int64_t));
    if (Uc > 0) {
        int *ur = realloc(lu->U_row_idx, (size_t)Uc * sizeof(int));
        double *uv = realloc(lu->U_values, (size_t)Uc * sizeof(double));
        if (!ur || !uv) { free(inv_row); free(inv_col); return 1001; }
        lu->U_row_idx = ur; lu->U_values = uv;
    }
    /* Ui[k] = step (row in step space), Uj[k] = original col */
    for (int k = 0; k < Uc; k++) lu->U_col_ptr[Ui[k] + 1]++;
    for (int j = 1; j <= m; j++) lu->U_col_ptr[j] += lu->U_col_ptr[j - 1];
    wp = calloc((size_t)m, sizeof(int64_t));
    if (!wp) { free(inv_row); free(inv_col); return 1001; }
    for (int k = 0; k < Uc; k++) {
        int row_step = Ui[k];
        int64_t pos = lu->U_col_ptr[row_step] + wp[row_step];
        lu->U_row_idx[pos] = inv_col[Uj[k]];  /* Convert col to step space */
        lu->U_values[pos] = Uv[k];
        wp[row_step]++;
    }
    free(wp);
    free(inv_row);
    free(inv_col);
    return 0;
}
