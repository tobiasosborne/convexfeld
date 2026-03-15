/**
 * @file lu_factorize.c
 * @brief Sparse Markowitz-ordered LU factorization for basis matrix.
 *
 * Sparse Gaussian elimination with threshold pivoting per
 * Suhl & Suhl (1990) and Maros (2003) Chapter 5. Dense phase
 * transition when active submatrix density exceeds 40%.
 *
 * Spec: product_form_inverse.md Step 1, numerical_stability.md Section D
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

#define MIN_PIVOT       CXF_MIN_PIVOT      /* 1e-13 */
#define DENSE_THRESHOLD 0.4
#define GROWTH_LIMIT    1e8
#define MARKOWITZ_MAX   0.99   /* Upper bound for adaptive Markowitz tol */

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

    /* Adaptive Markowitz tolerance (numerical_stability.md §D) */
    double mtol = basis->markowitz_tol;
    if (mtol < CXF_MARKOWITZ_TOL) mtol = CXF_MARKOWITZ_TOL;

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
        if (sparse_find_pivot(sw, mtol, &piv_row, &piv_col, &piv_val)) {
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
            if (dense_find_pivot(D, dn, mtol, d_relim, d_celim, &dr, &dc, &dv)) {
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

    /* Growth factor monitoring (numerical_stability.md Section D) */
    if (max_initial > 0.0 && max_u / max_initial > GROWTH_LIMIT) {
        basis->numerical_flag = 1;
        /* Adaptive: increase Markowitz tol for next factorization */
        double new_tol = basis->markowitz_tol * 2.0;
        if (new_tol > MARKOWITZ_MAX) new_tol = MARKOWITZ_MAX;
        basis->markowitz_tol = new_tol;
    }

cleanup:
    sparse_work_free(sw);
    free(Li); free(Lj); free(Lv);
    free(Ui); free(Uj); free(Uv);
    free(pr_cols); free(pr_vals);
    return rc;
}
