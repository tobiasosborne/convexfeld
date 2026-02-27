/**
 * @file lu_factorize.c
 * @brief Sparse Markowitz-ordered LU factorization for basis matrix.
 *
 * Sparse Gaussian elimination with threshold pivoting per
 * Suhl & Suhl (1990) and Maros (2003) Chapter 5. Dense phase
 * transition when active submatrix density exceeds 40%.
 *
 * Spec: product_form_inverse.md Step 1, numerical_stability.md §D
 */

#include "basis_internal.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define MARKOWITZ_THRESHOLD CXF_MARKOWITZ_TOL  /* 1/128 */
#define MIN_PIVOT           CXF_MIN_PIVOT      /* 1e-13 */
#define DENSE_THRESHOLD     0.4
#define GROWTH_LIMIT        1e8

/* --- Dense phase helpers (for small remaining submatrix) --- */

static int dense_find_pivot(const double *D, int n,
                            const int *relim, const int *celim,
                            int *out_r, int *out_c, double *out_v) {
    int best_r = -1, best_c = -1;
    int64_t best_score = (int64_t)(n + 1) * (int64_t)(n + 1);
    double best_abs = 0.0;

    for (int j = 0; j < n; j++) {
        if (celim[j]) continue;
        double cmax = 0.0;
        int col_cnt = 0;
        for (int i = 0; i < n; i++) {
            if (relim[i]) continue;
            double av = fabs(D[i * n + j]);
            if (av >= MIN_PIVOT) { col_cnt++; if (av > cmax) cmax = av; }
        }
        if (col_cnt == 0) continue;
        double thr = MARKOWITZ_THRESHOLD * cmax;
        for (int i = 0; i < n; i++) {
            if (relim[i]) continue;
            double av = fabs(D[i * n + j]);
            if (av < thr) continue;
            int r_cnt = 0;
            for (int jj = 0; jj < n; jj++)
                if (!celim[jj] && fabs(D[i * n + jj]) >= MIN_PIVOT) r_cnt++;
            int64_t sc = (int64_t)(r_cnt - 1) * (int64_t)(col_cnt - 1);
            if (sc < best_score || (sc == best_score && av > best_abs)) {
                best_score = sc; best_r = i; best_c = j; best_abs = av;
            }
        }
    }
    *out_r = best_r; *out_c = best_c;
    *out_v = (best_r >= 0) ? D[best_r * n + best_c] : 0.0;
    return (best_r >= 0 && best_abs >= MIN_PIVOT) ? 0 : -1;
}

static int dense_eliminate(double *D, int n, int piv_r, int piv_c,
                           double piv_val, int *relim, int *celim,
                           int **L_i, int **L_j, double **L_v,
                           int *Lc, int *Lcap, int step) {
    relim[piv_r] = 1;
    celim[piv_c] = 1;
    for (int i = 0; i < n; i++) {
        if (relim[i] || i == piv_r) continue;
        double val = D[i * n + piv_c];
        if (fabs(val) < MIN_PIVOT) continue;
        double mult = val / piv_val;
        if (*Lc >= *Lcap) {
            int nc = *Lcap * 2;
            int *ni = realloc(*L_i, (size_t)nc * sizeof(int));
            int *nj = realloc(*L_j, (size_t)nc * sizeof(int));
            double *nv = realloc(*L_v, (size_t)nc * sizeof(double));
            if (!ni || !nj || !nv) return 1001;
            *L_i = ni; *L_j = nj; *L_v = nv; *Lcap = nc;
        }
        (*L_i)[*Lc] = i; (*L_j)[*Lc] = step; (*L_v)[*Lc] = mult;
        (*Lc)++;
        D[i * n + piv_c] = 0.0;
        for (int jj = 0; jj < n; jj++) {
            if (celim[jj] || jj == piv_c) continue;
            D[i * n + jj] -= mult * D[piv_r * n + jj];
        }
    }
    return 0;
}

/* --- CSC assembly from COO arrays --- */

static int build_lu_output(LUFactors *lu, int m,
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

/* --- Main entry point --- */

int cxf_lu_factorize(LUFactors *lu, SolverState *ctx) {
    if (!lu || !ctx || !ctx->basis) return CXF_ERROR_NULL_ARGUMENT;
    BasisState *basis = ctx->basis;
    int m = basis->m;
    if (m == 0) { lu->valid = 1; return 0; }
    if (!ctx->csc_col_ptr) return CXF_ERROR_NULL_ARGUMENT;

    SparseWork *sw = sparse_work_create(m);
    if (!sw) return 1001;
    int rc = sparse_work_extract(sw, ctx);
    if (rc) { sparse_work_free(sw); return rc; }

    /* COO arrays for L and U entries */
    int Lcap = m * 2, Lc = 0;
    int *Li = malloc((size_t)Lcap * sizeof(int));
    int *Lj = malloc((size_t)Lcap * sizeof(int));
    double *Lv = malloc((size_t)Lcap * sizeof(double));
    int Ucap = m * 2, Uc = 0;
    int *Ui = malloc((size_t)Ucap * sizeof(int));
    int *Uj = malloc((size_t)Ucap * sizeof(int));
    double *Uv = malloc((size_t)Ucap * sizeof(double));
    int *pr_cols = malloc((size_t)m * sizeof(int));
    double *pr_vals = malloc((size_t)m * sizeof(double));

    if (!Li || !Lj || !Lv || !Ui || !Uj || !Uv || !pr_cols || !pr_vals) {
        rc = 1001; goto cleanup;
    }

    /* Track growth factor: max |A_ij| from initial matrix */
    double max_initial = 0.0;
    for (int j = 0; j < m; j++)
        if (sw->col_max[j] > max_initial) max_initial = sw->col_max[j];
    double max_u = 0.0;

    int step = 0;
    /* === Sparse phase === */
    for (; step < m; step++) {
        if (sparse_work_density(sw) > DENSE_THRESHOLD && sw->active_count > 1)
            break;

        int piv_row, piv_col; double piv_val;
        if (sparse_find_pivot(sw, &piv_row, &piv_col, &piv_val)) {
            rc = 3; goto cleanup;  /* Singular */
        }
        lu->perm_row[step] = piv_row;
        lu->perm_col[step] = piv_col;
        lu->U_diag[step] = piv_val;
        if (fabs(piv_val) > max_u) max_u = fabs(piv_val);

        int pr_len = sparse_extract_pivot_row(sw, piv_row, pr_cols, pr_vals);

        /* Collect U entries from pivot row (excluding pivot column) */
        for (int p = 0; p < pr_len; p++) {
            if (pr_cols[p] == piv_col) continue;
            if (Uc >= Ucap) {
                int nc = Ucap * 2;
                int *ni = realloc(Ui, (size_t)nc * sizeof(int));
                int *nj = realloc(Uj, (size_t)nc * sizeof(int));
                double *nv = realloc(Uv, (size_t)nc * sizeof(double));
                if (!ni || !nj || !nv) { rc = 1001; goto cleanup; }
                Ui = ni; Uj = nj; Uv = nv; Ucap = nc;
            }
            Ui[Uc] = step;
            Uj[Uc] = pr_cols[p];  /* Original column, converted later */
            Uv[Uc] = pr_vals[p];
            Uc++;
        }

        rc = sparse_eliminate(sw, piv_row, piv_col, piv_val,
                              pr_cols, pr_vals, pr_len,
                              &Li, &Lj, &Lv, &Lc, &Lcap, step);
        if (rc) goto cleanup;
    }

    /* === Dense phase (if triggered) === */
    if (step < m && sw->active_count > 0) {
        int dn = sw->active_count;
        double *D = calloc((size_t)dn * (size_t)dn, sizeof(double));
        int *map_r = malloc((size_t)dn * sizeof(int));
        int *map_c = malloc((size_t)dn * sizeof(int));
        int *d_relim = calloc((size_t)dn, sizeof(int));
        int *d_celim = calloc((size_t)dn, sizeof(int));
        if (!D || !map_r || !map_c || !d_relim || !d_celim) {
            free(D); free(map_r); free(map_c);
            free(d_relim); free(d_celim);
            rc = 1001; goto cleanup;
        }
        sparse_to_dense(sw, D, map_r, map_c);

        for (int ds = 0; step < m; step++, ds++) {
            int dr, dc; double dv;
            if (dense_find_pivot(D, dn, d_relim, d_celim, &dr, &dc, &dv)) {
                free(D); free(map_r); free(map_c);
                free(d_relim); free(d_celim);
                rc = 3; goto cleanup;
            }
            lu->perm_row[step] = map_r[dr];
            lu->perm_col[step] = map_c[dc];
            lu->U_diag[step] = dv;
            if (fabs(dv) > max_u) max_u = fabs(dv);

            /* Collect dense U entries */
            for (int jj = 0; jj < dn; jj++) {
                if (d_celim[jj] || jj == dc) continue;
                double uval = D[dr * dn + jj];
                if (fabs(uval) < MIN_PIVOT) continue;
                if (Uc >= Ucap) {
                    int nc = Ucap * 2;
                    int *ni = realloc(Ui, (size_t)nc * sizeof(int));
                    int *nj = realloc(Uj, (size_t)nc * sizeof(int));
                    double *nv = realloc(Uv, (size_t)nc * sizeof(double));
                    if (!ni || !nj || !nv) {
                        free(D); free(map_r); free(map_c);
                        free(d_relim); free(d_celim);
                        rc = 1001; goto cleanup;
                    }
                    Ui = ni; Uj = nj; Uv = nv; Ucap = nc;
                }
                Ui[Uc] = step;
                Uj[Uc] = map_c[jj];  /* Original column */
                Uv[Uc] = uval;
                Uc++;
            }

            rc = dense_eliminate(D, dn, dr, dc, dv, d_relim, d_celim,
                                &Li, &Lj, &Lv, &Lc, &Lcap, step);
            if (rc) {
                free(D); free(map_r); free(map_c);
                free(d_relim); free(d_celim);
                goto cleanup;
            }

            /* Fix L entries: dense_eliminate stored dense row indices,
             * but we need original row indices. Patch the last batch. */
            for (int k = Lc - 1; k >= 0 && Lj[k] == step; k--)
                Li[k] = map_r[Li[k]];
        }
        free(D); free(map_r); free(map_c);
        free(d_relim); free(d_celim);
    }

    rc = build_lu_output(lu, m, Li, Lj, Lv, Lc, Ui, Uj, Uv, Uc);
    if (rc == 0) lu->valid = 1;

    /* Growth factor monitoring (numerical_stability.md §D) */
    if (max_initial > 0.0 && max_u / max_initial > GROWTH_LIMIT)
        basis->numerical_flag = 1;

cleanup:
    sparse_work_free(sw);
    free(Li); free(Lj); free(Lv);
    free(Ui); free(Uj); free(Uv);
    free(pr_cols); free(pr_vals);
    return rc;
}
