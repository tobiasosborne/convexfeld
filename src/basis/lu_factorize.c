/**
 * @file lu_factorize.c
 * @brief Markowitz-ordered LU factorization for basis matrix.
 *
 * P2.1 (uxae): Optimized pivot search with maintained column maxima.
 * Eliminates the O(m) col_max scan per column per step, reducing
 * overall complexity from O(m^4) to O(m^2 * avg_nnz).
 *
 * Spec: product_form_inverse.md Step 1
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MARKOWITZ_THRESHOLD CXF_MARKOWITZ_TOL   /* 1/128 */
#define MIN_PIVOT           CXF_MIN_PIVOT       /* 1e-13 */

/**
 * @brief Extract basis matrix into dense format and compute initial counts.
 * @return 0 on success, nonzero on error.
 */
static int extract_basis_matrix(double *B, int *row_count, int *col_count,
                                double *col_max, SolverState *ctx) {
    BasisState *basis = ctx->basis;
    int m = basis->m;
    int n_orig = ctx->num_vars;

    /* Extract basis columns into dense B (using SolverState's CSC copy) */
    for (int j = 0; j < m; j++) {
        int var = basis->basic_vars[j];
        if (var < n_orig && ctx->csc_col_ptr != NULL) {
            for (int64_t k = ctx->csc_col_ptr[var];
                 k < ctx->csc_col_ptr[var + 1]; k++) {
                int row = ctx->csc_row_idx[k];
                if (row < m) B[row * m + j] = ctx->csc_values[k];
            }
        } else if (var < n_orig + m) {
            /* Slack/surplus variable: use diag_coeff */
            int slack_row = var - n_orig;
            if (slack_row >= 0 && slack_row < m)
                B[slack_row * m + j] = basis->diag_coeff[slack_row];
        }
        /* No artificial variable case — implicit Phase I has no artificials */
    }

    /* Count nonzeros and compute column maxima */
    for (int j = 0; j < m; j++) {
        double cmax = 0.0;
        for (int i = 0; i < m; i++) {
            double val = fabs(B[i * m + j]);
            if (val > MIN_PIVOT) {
                row_count[i]++;
                col_count[j]++;
                if (val > cmax) cmax = val;
            }
        }
        col_max[j] = cmax;
    }
    return 0;
}

/**
 * @brief Recompute column maximum for a single column after elimination.
 */
static double recompute_col_max(const double *B, int m, int col,
                                const int *row_elim) {
    double cmax = 0.0;
    for (int i = 0; i < m; i++) {
        if (row_elim[i]) continue;
        double val = fabs(B[i * m + col]);
        if (val > cmax) cmax = val;
    }
    return cmax;
}

/**
 * @brief Find Markowitz pivot using pre-computed column maxima.
 *
 * This is the key optimization: col_max[j] is read directly instead
 * of scanning all rows per column. Reduces pivot search from O(m^2)
 * to O(active_cols * active_rows_with_nonzero_in_col).
 */
static int find_pivot(const double *B, int m,
                      const int *row_count, const int *col_count,
                      const double *col_max,
                      const int *row_elim, const int *col_elim,
                      int *out_row, int *out_col, double *out_pivot) {
    int best_row = -1, best_col = -1;
    int64_t best_score = (int64_t)(m + 1) * (int64_t)(m + 1);
    double best_pivot = 0.0;

    for (int j = 0; j < m; j++) {
        if (col_elim[j]) continue;
        if (col_max[j] < MIN_PIVOT) continue;

        double threshold = MARKOWITZ_THRESHOLD * col_max[j];

        for (int i = 0; i < m; i++) {
            if (row_elim[i]) continue;
            double val = fabs(B[i * m + j]);
            if (val < threshold) continue;

            int64_t score = (int64_t)(row_count[i] - 1) *
                            (int64_t)(col_count[j] - 1);

            if (score < best_score ||
                (score == best_score && val > fabs(best_pivot))) {
                best_score = score;
                best_row = i;
                best_col = j;
                best_pivot = B[i * m + j];
            }
        }
    }

    *out_row = best_row;
    *out_col = best_col;
    *out_pivot = best_pivot;
    return (best_row >= 0 && fabs(best_pivot) >= MIN_PIVOT) ? 0 : -1;
}

/**
 * @brief Perform one elimination step, updating B, counts, and col_max.
 */
static int eliminate_step(double *B, int m,
                          int *row_count, int *col_count, double *col_max,
                          int *row_elim, int *col_elim,
                          int piv_row, int piv_col, double piv_val,
                          int **L_i, int **L_j, double **L_v,
                          int *L_count, int *L_cap, int step) {
    row_elim[piv_row] = 1;
    col_elim[piv_col] = 1;
    col_max[piv_col] = 0.0;
    col_count[piv_col] = 0;

    /* Track which columns are modified (pivot row has nonzero there) */
    for (int i = 0; i < m; i++) {
        if (row_elim[i] || i == piv_row) continue;
        double val = B[i * m + piv_col];
        if (fabs(val) < MIN_PIVOT) continue;

        double mult = val / piv_val;

        /* Store L entry */
        if (*L_count >= *L_cap) {
            *L_cap *= 2;
            int *ni = realloc(*L_i, (size_t)*L_cap * sizeof(int));
            int *nj = realloc(*L_j, (size_t)*L_cap * sizeof(int));
            double *nv = realloc(*L_v, (size_t)*L_cap * sizeof(double));
            if (!ni || !nj || !nv) return 1001;
            *L_i = ni; *L_j = nj; *L_v = nv;
        }
        (*L_i)[*L_count] = i;
        (*L_j)[*L_count] = step;
        (*L_v)[*L_count] = mult;
        (*L_count)++;

        /* Update row i */
        B[i * m + piv_col] = 0.0;
        row_count[i]--;
        for (int jj = 0; jj < m; jj++) {
            if (col_elim[jj]) continue;
            double pv = B[piv_row * m + jj];
            if (fabs(pv) < MIN_PIVOT) continue;

            double old_val = B[i * m + jj];
            double new_val = old_val - mult * pv;

            if (fabs(old_val) < MIN_PIVOT && fabs(new_val) >= MIN_PIVOT) {
                row_count[i]++;
                col_count[jj]++;
            } else if (fabs(old_val) >= MIN_PIVOT && fabs(new_val) < MIN_PIVOT) {
                row_count[i]--;
                col_count[jj]--;
            }
            B[i * m + jj] = new_val;
        }
    }

    /* Update column maxima for modified columns.
     * Only columns where the pivot row had a nonzero are affected. */
    for (int jj = 0; jj < m; jj++) {
        if (col_elim[jj]) continue;
        if (fabs(B[piv_row * m + jj]) < MIN_PIVOT) continue;
        col_max[jj] = recompute_col_max(B, m, jj, row_elim);
    }

    return 0;
}

/**
 * @brief Build sparse L and U output from elimination results.
 */
static int build_lu_output(LUFactors *lu, const double *B, int m,
                           const int *L_i_arr, const int *L_j_arr,
                           const double *L_v_arr, int L_count) {
    /* Build U: off-diagonal entries from pivot rows */
    lu->U_nnz = 0;
    for (int step = 0; step < m; step++) {
        int piv_row = lu->perm_row[step];
        lu->U_col_ptr[step] = lu->U_nnz;
        for (int j_step = step + 1; j_step < m; j_step++) {
            int col = lu->perm_col[j_step];
            if (fabs(B[piv_row * m + col]) >= MIN_PIVOT)
                lu->U_nnz++;
        }
    }
    lu->U_col_ptr[m] = lu->U_nnz;

    if (lu->U_nnz > 0) {
        int *ur = realloc(lu->U_row_idx, (size_t)lu->U_nnz * sizeof(int));
        double *uv = realloc(lu->U_values, (size_t)lu->U_nnz * sizeof(double));
        if (!ur || !uv) return 1001;
        lu->U_row_idx = ur;
        lu->U_values = uv;

        int64_t idx = 0;
        for (int step = 0; step < m; step++) {
            int piv_row = lu->perm_row[step];
            for (int j_step = step + 1; j_step < m; j_step++) {
                int col = lu->perm_col[j_step];
                double val = B[piv_row * m + col];
                if (fabs(val) >= MIN_PIVOT) {
                    lu->U_row_idx[idx] = j_step;
                    lu->U_values[idx] = val;
                    idx++;
                }
            }
        }
    }

    /* Build L in column-wise format */
    lu->L_nnz = (int64_t)L_count;
    if (L_count > 0) {
        int *lr = realloc(lu->L_row_idx, (size_t)L_count * sizeof(int));
        double *lv = realloc(lu->L_values, (size_t)L_count * sizeof(double));
        if (!lr || !lv) return 1001;
        lu->L_row_idx = lr;
        lu->L_values = lv;
    }

    memset(lu->L_col_ptr, 0, (size_t)(m + 1) * sizeof(int64_t));
    for (int k = 0; k < L_count; k++)
        lu->L_col_ptr[L_j_arr[k] + 1]++;
    for (int j = 1; j <= m; j++)
        lu->L_col_ptr[j] += lu->L_col_ptr[j - 1];

    int64_t *wp = calloc((size_t)m, sizeof(int64_t));
    if (!wp) return 1001;
    for (int k = 0; k < L_count; k++) {
        int col = L_j_arr[k];
        int64_t pos = lu->L_col_ptr[col] + wp[col];
        lu->L_row_idx[pos] = L_i_arr[k];
        lu->L_values[pos] = L_v_arr[k];
        wp[col]++;
    }
    free(wp);

    /* Convert L row indices from original to step positions */
    int *inv_perm = malloc((size_t)m * sizeof(int));
    if (!inv_perm) return 1001;
    for (int k = 0; k < m; k++)
        inv_perm[lu->perm_row[k]] = k;
    for (int64_t p = 0; p < lu->L_nnz; p++)
        lu->L_row_idx[p] = inv_perm[lu->L_row_idx[p]];
    free(inv_perm);

    return 0;
}

/**
 * @brief Compute LU factorization of basis matrix.
 *
 * Uses dense working matrix with Markowitz pivot selection and
 * maintained column maxima for O(m^2 * avg_nnz) complexity.
 */
int cxf_lu_factorize(LUFactors *lu, SolverState *ctx) {
    if (!lu || !ctx || !ctx->basis) return CXF_ERROR_NULL_ARGUMENT;

    BasisState *basis = ctx->basis;
    int m = basis->m;
    if (m == 0) { lu->valid = 1; return 0; }

    if (!ctx->csc_col_ptr) return CXF_ERROR_NULL_ARGUMENT;

    /* Allocate working storage */
    double *B = calloc((size_t)m * m, sizeof(double));
    int *row_count = calloc((size_t)m, sizeof(int));
    int *col_count = calloc((size_t)m, sizeof(int));
    double *col_max = calloc((size_t)m, sizeof(double));
    int *row_elim = calloc((size_t)m, sizeof(int));
    int *col_elim = calloc((size_t)m, sizeof(int));

    if (!B || !row_count || !col_count || !col_max || !row_elim || !col_elim) {
        free(B); free(row_count); free(col_count);
        free(col_max); free(row_elim); free(col_elim);
        return 1001;
    }

    extract_basis_matrix(B, row_count, col_count, col_max, ctx);

    /* L entries stored in COO for assembly later */
    int L_cap = m * 2;
    int *L_i = malloc((size_t)L_cap * sizeof(int));
    int *L_j = malloc((size_t)L_cap * sizeof(int));
    double *L_v = malloc((size_t)L_cap * sizeof(double));
    int L_count = 0;
    int rc = 0;

    if (!L_i || !L_j || !L_v) { rc = 1001; goto cleanup; }

    /* Main factorization loop */
    for (int step = 0; step < m; step++) {
        int piv_row, piv_col;
        double piv_val;

        if (find_pivot(B, m, row_count, col_count, col_max,
                       row_elim, col_elim, &piv_row, &piv_col, &piv_val)) {
            rc = 3;  /* Singular */
            goto cleanup;
        }

        lu->perm_row[step] = piv_row;
        lu->perm_col[step] = piv_col;
        lu->U_diag[step] = piv_val;

        rc = eliminate_step(B, m, row_count, col_count, col_max,
                           row_elim, col_elim, piv_row, piv_col, piv_val,
                           &L_i, &L_j, &L_v, &L_count, &L_cap, step);
        if (rc) goto cleanup;
    }

    rc = build_lu_output(lu, B, m, L_i, L_j, L_v, L_count);
    if (rc == 0) lu->valid = 1;

cleanup:
    free(B); free(row_count); free(col_count);
    free(col_max); free(row_elim); free(col_elim);
    free(L_i); free(L_j); free(L_v);
    return rc;
}
