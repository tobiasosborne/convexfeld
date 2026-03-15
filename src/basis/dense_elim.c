/**
 * @file dense_elim.c
 * @brief Dense Markowitz pivot search and Gaussian elimination.
 *
 * Dense phase helpers for LU factorization. Used when the active
 * submatrix density exceeds the threshold and the remaining
 * elimination is performed on a dense array.
 *
 * Spec: numerical_stability.md Section D (Markowitz + threshold pivoting)
 */

#include "basis_internal.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

#define MARKOWITZ_TOL CXF_MARKOWITZ_TOL  /* 1/128 */
#define MIN_PIVOT     CXF_MIN_PIVOT      /* 1e-13 */

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

int dense_find_pivot(const double *D, int n, double markowitz_tol,
                     const int *relim, const int *celim,
                     int *out_r, int *out_c, double *out_v) {
    int best_r = -1, best_c = -1;
    int64_t best_score = (int64_t)(n + 1) * (int64_t)(n + 1);
    double best_rel = 0.0;   /* |a_ij| / col_max: scale-independent */
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
        if (col_cnt == 0 || cmax < MIN_PIVOT) continue;
        double thr = markowitz_tol * cmax;
        for (int i = 0; i < n; i++) {
            if (relim[i]) continue;
            double av = fabs(D[i * n + j]);
            if (av < thr) continue;
            int r_cnt = 0;
            for (int jj = 0; jj < n; jj++)
                if (!celim[jj] && fabs(D[i * n + jj]) >= MIN_PIVOT) r_cnt++;
            int64_t sc = (int64_t)(r_cnt - 1) * (int64_t)(col_cnt - 1);
            double rel = av / cmax;
            if (sc < best_score || (sc == best_score && rel > best_rel)) {
                best_score = sc; best_r = i; best_c = j;
                best_rel = rel; best_abs = av;
            }
        }
    }
    *out_r = best_r; *out_c = best_c;
    *out_v = (best_r >= 0) ? D[best_r * n + best_c] : 0.0;
    return (best_r >= 0 && best_abs >= MIN_PIVOT) ? 0 : -1;
}

int dense_eliminate(double *D, int n, int piv_r, int piv_c,
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
            int rc = grow_coo(L_i, L_j, L_v, Lcap);
            if (rc) return rc;
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
