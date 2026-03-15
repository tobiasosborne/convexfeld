/**
 * @file sparse_elim.c
 * @brief Sparse Markowitz pivot search and Gaussian elimination.
 *
 * Markowitz (1957) pivot ordering with threshold pivoting per
 * Suhl & Suhl (1990). Right-looking elimination updates the
 * active submatrix after each pivot. Maros (2003) Ch. 5.
 *
 * Spec: numerical_stability.md §D (Markowitz + threshold pivoting)
 */

#include "basis_internal.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

#define MARKOWITZ_TOL CXF_MARKOWITZ_TOL  /* 1/128 ≈ 0.0078 */
#define MIN_PIVOT     CXF_MIN_PIVOT      /* 1e-13 */

int sparse_find_pivot(const SparseWork *sw, double markowitz_tol,
                      int *out_row, int *out_col, double *out_val) {
    int best_row = -1, best_col = -1;
    int64_t best_score = (int64_t)(sw->m + 1) * (int64_t)(sw->m + 1);
    double best_rel = 0.0;   /* |a_ij| / col_max[j]: scale-independent */
    double best_abs = 0.0;

    for (int j = 0; j < sw->m; j++) {
        if (!sw->col_active[j]) continue;
        double cmax = sw->col_max[j];
        if (cmax < MIN_PIVOT) continue;
        double threshold = markowitz_tol * cmax;

        /* Count active entries in this column for Markowitz score */
        const SparseCol *col = &sw->cols[j];
        int col_active_len = 0;
        for (int k = 0; k < col->len; k++) {
            if (sw->row_active[col->row_idx[k]] &&
                fabs(col->values[k]) >= MIN_PIVOT)
                col_active_len++;
        }
        if (col_active_len == 0) continue;

        for (int k = 0; k < col->len; k++) {
            int i = col->row_idx[k];
            if (!sw->row_active[i]) continue;
            double av = fabs(col->values[k]);
            if (av < threshold) continue;

            int64_t score = (int64_t)(sw->row_count[i] - 1) *
                            (int64_t)(col_active_len - 1);
            double rel = av / cmax;
            if (score < best_score ||
                (score == best_score && rel > best_rel)) {
                best_score = score;
                best_row = i;
                best_col = j;
                best_rel = rel;
                best_abs = av;
            }
        }
    }

    *out_row = best_row;
    *out_col = best_col;
    *out_val = (best_row >= 0 && best_col >= 0)
             ? sw->cols[best_col].values[0] : 0.0;
    /* Retrieve actual value at (best_row, best_col) */
    if (best_col >= 0) {
        const SparseCol *col = &sw->cols[best_col];
        for (int k = 0; k < col->len; k++) {
            if (col->row_idx[k] == best_row) {
                *out_val = col->values[k];
                break;
            }
        }
    }
    return (best_row >= 0 && best_abs >= MIN_PIVOT) ? 0 : -1;
}

/** Recompute col_max for a single column from active entries. */
static double recompute_col_max(const SparseCol *col,
                                const int *row_active) {
    double cmax = 0.0;
    for (int k = 0; k < col->len; k++) {
        if (!row_active[col->row_idx[k]]) continue;
        double av = fabs(col->values[k]);
        if (av > cmax) cmax = av;
    }
    return cmax;
}

/** Grow a COO array if at capacity. */
static int grow_coo(int **arr_i, int **arr_j, double **arr_v,
                    int *cap) {
    int nc = *cap * 2;
    int *ni = realloc(*arr_i, (size_t)nc * sizeof(int));
    int *nj = realloc(*arr_j, (size_t)nc * sizeof(int));
    double *nv = realloc(*arr_v, (size_t)nc * sizeof(double));
    if (!ni || !nj || !nv) return 1001;
    *arr_i = ni; *arr_j = nj; *arr_v = nv; *cap = nc;
    return 0;
}

int sparse_eliminate(SparseWork *sw, int piv_row, int piv_col,
                     double piv_val,
                     const int *pr_cols, const double *pr_vals, int pr_len,
                     int **L_i, int **L_j, double **L_v,
                     int *L_count, int *L_cap, int step) {
    SparseCol *pcol = &sw->cols[piv_col];

    /* For each active row with nonzero in pivot column */
    for (int k = 0; k < pcol->len; k++) {
        int i = pcol->row_idx[k];
        if (!sw->row_active[i] || i == piv_row) continue;
        if (fabs(pcol->values[k]) < MIN_PIVOT) continue;

        double mult = pcol->values[k] / piv_val;

        /* Store L entry */
        if (*L_count >= *L_cap) {
            int rc = grow_coo(L_i, L_j, L_v, L_cap);
            if (rc) return rc;
        }
        (*L_i)[*L_count] = i;
        (*L_j)[*L_count] = step;
        (*L_v)[*L_count] = mult;
        (*L_count)++;

        /* Update row i across pivot row columns */
        for (int p = 0; p < pr_len; p++) {
            int jj = pr_cols[p];
            if (jj == piv_col) continue;
            double pv = pr_vals[p];

            /* Find entry (i, jj) in column jj */
            SparseCol *cj = &sw->cols[jj];
            int found = -1;
            for (int q = 0; q < cj->len; q++) {
                if (cj->row_idx[q] == i) { found = q; break; }
            }

            if (found >= 0) {
                double old_val = cj->values[found];
                double new_val = old_val - mult * pv;
                int was_live = fabs(old_val) >= MIN_PIVOT;
                int is_live = fabs(new_val) >= MIN_PIVOT;
                cj->values[found] = is_live ? new_val : 0.0;
                if (was_live && !is_live) {
                    sw->row_count[i]--;
                    sw->total_nnz--;
                } else if (!was_live && is_live) {
                    sw->row_count[i]++;
                    sw->total_nnz++;
                }
            } else {
                /* Fill-in: new entry */
                double new_val = -mult * pv;
                if (fabs(new_val) >= MIN_PIVOT) {
                    int rc = sparse_col_append(cj, i, new_val);
                    if (rc) return rc;
                    sw->row_count[i]++;
                    sw->total_nnz++;
                }
            }
        }
    }

    /* Zero pivot column entries for eliminated row, update counts */
    for (int k = 0; k < pcol->len; k++) {
        int i = pcol->row_idx[k];
        if (sw->row_active[i] && fabs(pcol->values[k]) >= MIN_PIVOT) {
            sw->row_count[i]--;
            sw->total_nnz--;
        }
        pcol->values[k] = 0.0;
    }

    /* Mark pivot row and column inactive */
    sw->row_active[piv_row] = 0;
    sw->col_active[piv_col] = 0;
    sw->col_max[piv_col] = 0.0;
    sw->active_count--;

    /* Recompute col_max only for columns that had pivot row entries */
    for (int p = 0; p < pr_len; p++) {
        int jj = pr_cols[p];
        if (jj == piv_col || !sw->col_active[jj]) continue;
        sw->col_max[jj] = recompute_col_max(&sw->cols[jj], sw->row_active);
    }

    return 0;
}
