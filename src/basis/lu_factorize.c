/**
 * @file lu_factorize.c
 * @brief Markowitz-ordered LU factorization for basis matrix.
 *
 * Implements sparse LU factorization with Markowitz pivot selection
 * and threshold pivoting for numerical stability.
 *
 * Spec: docs/specs/functions/basis/cxf_basis_refactor.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Threshold for pivot acceptance: |pivot| >= threshold * max_in_col */
#define MARKOWITZ_THRESHOLD 0.1
#define MIN_PIVOT 1e-12

/**
 * @brief Compute LU factorization of basis matrix.
 *
 * Uses dense working matrix with Markowitz pivot selection.
 * Output is stored in sparse LUFactors structure.
 *
 * @param lu LUFactors structure to store results (must be pre-allocated).
 * @param ctx SolverContext with basis and matrix access.
 * @return 0 on success, 3 for singular matrix, 1001 for OOM.
 */
int cxf_lu_factorize(LUFactors *lu, SolverContext *ctx) {
    if (lu == NULL || ctx == NULL || ctx->basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    BasisState *basis = ctx->basis;
    int m = basis->m;

    if (m == 0) {
        lu->valid = 1;
        return 0;
    }

    CxfModel *model = ctx->model_ref;
    if (model == NULL || model->matrix == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    SparseMatrix *A = model->matrix;
    int n_orig = ctx->num_vars;

    /* Allocate dense working matrix B[m][m] */
    double *B = (double *)calloc((size_t)m * (size_t)m, sizeof(double));
    if (B == NULL) {
        return 1001;  /* Out of memory */
    }

    /* Track row/col counts for Markowitz, and elimination state */
    int *row_count = (int *)calloc((size_t)m, sizeof(int));
    int *col_count = (int *)calloc((size_t)m, sizeof(int));
    int *row_elim = (int *)calloc((size_t)m, sizeof(int));  /* 1 if row eliminated */
    int *col_elim = (int *)calloc((size_t)m, sizeof(int));  /* 1 if col eliminated */

    if (row_count == NULL || col_count == NULL ||
        row_elim == NULL || col_elim == NULL) {
        free(B); free(row_count); free(col_count);
        free(row_elim); free(col_elim);
        return 1001;
    }

    /* Extract basis columns into dense B matrix.
     * Column j of B corresponds to basic variable basis->basic_vars[j].
     * - If var < n_orig: extract from constraint matrix A
     * - If var >= n_orig: slack variable, unit vector at row (var - n_orig) */
    for (int j = 0; j < m; j++) {
        int var = basis->basic_vars[j];

        if (var < n_orig) {
            /* Structural variable - extract from A */
            for (int64_t k = A->col_ptr[var]; k < A->col_ptr[var + 1]; k++) {
                int row = A->row_idx[k];
                if (row < m) {
                    B[row * m + j] = A->values[k];
                }
            }
        } else {
            /* Slack variable - unit vector at row (var - n_orig) */
            int slack_row = var - n_orig;
            if (slack_row >= 0 && slack_row < m) {
                /* Slack coefficient is based on constraint sense.
                 * Use diag_coeff which already accounts for this. */
                B[slack_row * m + j] = basis->diag_coeff[slack_row];
            }
        }
    }

    /* Count non-zeros per row and column */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            if (fabs(B[i * m + j]) > MIN_PIVOT) {
                row_count[i]++;
                col_count[j]++;
            }
        }
    }

    /* Temporary storage for L entries (multipliers) */
    int L_cap = m * 2;  /* Initial estimate */
    int *L_i = (int *)malloc((size_t)L_cap * sizeof(int));
    int *L_j = (int *)malloc((size_t)L_cap * sizeof(int));
    double *L_v = (double *)malloc((size_t)L_cap * sizeof(double));
    int L_count = 0;

    if (L_i == NULL || L_j == NULL || L_v == NULL) {
        free(B); free(row_count); free(col_count);
        free(row_elim); free(col_elim);
        free(L_i); free(L_j); free(L_v);
        return 1001;
    }

    /* Markowitz LU factorization */
    for (int step = 0; step < m; step++) {
        /* Find pivot using Markowitz criterion with threshold */
        int best_row = -1, best_col = -1;
        int64_t best_score = (int64_t)(m + 1) * (int64_t)(m + 1);
        double best_pivot = 0.0;

        for (int j = 0; j < m; j++) {
            if (col_elim[j]) continue;

            /* Find max in this column for threshold */
            double col_max = 0.0;
            for (int i = 0; i < m; i++) {
                if (row_elim[i]) continue;
                double val = fabs(B[i * m + j]);
                if (val > col_max) col_max = val;
            }

            if (col_max < MIN_PIVOT) continue;  /* Skip zero column */

            double threshold = MARKOWITZ_THRESHOLD * col_max;

            /* Find best pivot in this column */
            for (int i = 0; i < m; i++) {
                if (row_elim[i]) continue;
                double val = fabs(B[i * m + j]);

                if (val >= threshold) {
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
        }

        if (best_row < 0 || fabs(best_pivot) < MIN_PIVOT) {
            /* Singular matrix */
            free(B); free(row_count); free(col_count);
            free(row_elim); free(col_elim);
            free(L_i); free(L_j); free(L_v);
            return 3;  /* Singular basis */
        }

        /* Record permutation */
        lu->perm_row[step] = best_row;
        lu->perm_col[step] = best_col;

        /* Store U diagonal */
        lu->U_diag[step] = best_pivot;

        /* Eliminate */
        row_elim[best_row] = 1;
        col_elim[best_col] = 1;

        for (int i = 0; i < m; i++) {
            if (row_elim[i]) continue;
            double val = B[i * m + best_col];
            if (fabs(val) < MIN_PIVOT) continue;

            double mult = val / best_pivot;

            /* Store L entry (multiplier) */
            if (L_count >= L_cap) {
                L_cap *= 2;
                int *new_i = realloc(L_i, (size_t)L_cap * sizeof(int));
                int *new_j = realloc(L_j, (size_t)L_cap * sizeof(int));
                double *new_v = realloc(L_v, (size_t)L_cap * sizeof(double));
                if (new_i == NULL || new_j == NULL || new_v == NULL) {
                    free(B); free(row_count); free(col_count);
                    free(row_elim); free(col_elim);
                    free(L_i); free(L_j); free(L_v);
                    return 1001;
                }
                L_i = new_i; L_j = new_j; L_v = new_v;
            }
            L_i[L_count] = i;
            L_j[L_count] = step;  /* Elimination step = L column */
            L_v[L_count] = mult;
            L_count++;

            /* Update row */
            B[i * m + best_col] = 0.0;
            row_count[i]--;
            for (int jj = 0; jj < m; jj++) {
                if (col_elim[jj]) continue;
                double piv_val = B[best_row * m + jj];
                if (fabs(piv_val) < MIN_PIVOT) continue;

                double old_val = B[i * m + jj];
                double new_val = old_val - mult * piv_val;

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

        col_count[best_col] = 0;  /* Column eliminated */
    }

    /* Extract U entries (off-diagonal) from remaining B */
    /* U is stored column-wise. For each elimination step, store non-zeros
     * in the pivot row that haven't been eliminated yet. */

    /* Count U non-zeros and build column pointers */
    lu->U_nnz = 0;
    for (int step = 0; step < m; step++) {
        int piv_row = lu->perm_row[step];
        lu->U_col_ptr[step] = lu->U_nnz;

        /* Count non-zeros in pivot row (excluding diagonal and eliminated cols) */
        for (int j_step = step + 1; j_step < m; j_step++) {
            int col = lu->perm_col[j_step];
            if (fabs(B[piv_row * m + col]) >= MIN_PIVOT) {
                lu->U_nnz++;
            }
        }
    }
    lu->U_col_ptr[m] = lu->U_nnz;

    /* Fill U values */
    if (lu->U_nnz > 0) {
        int64_t idx = 0;
        for (int step = 0; step < m; step++) {
            int piv_row = lu->perm_row[step];
            for (int j_step = step + 1; j_step < m; j_step++) {
                int col = lu->perm_col[j_step];
                double val = B[piv_row * m + col];
                if (fabs(val) >= MIN_PIVOT) {
                    lu->U_row_idx[idx] = j_step;  /* Store step index, not original row */
                    lu->U_values[idx] = val;
                    idx++;
                }
            }
        }
    }

    /* Build L in column-wise format */
    lu->L_nnz = (int64_t)L_count;
    memset(lu->L_col_ptr, 0, (size_t)(m + 1) * sizeof(int64_t));

    /* Count entries per column */
    for (int k = 0; k < L_count; k++) {
        lu->L_col_ptr[L_j[k] + 1]++;
    }

    /* Cumulative sum */
    for (int j = 1; j <= m; j++) {
        lu->L_col_ptr[j] += lu->L_col_ptr[j - 1];
    }

    /* Fill L entries */
    int64_t *work_ptr = (int64_t *)calloc((size_t)m, sizeof(int64_t));
    if (work_ptr == NULL) {
        free(B); free(row_count); free(col_count);
        free(row_elim); free(col_elim);
        free(L_i); free(L_j); free(L_v);
        return 1001;
    }

    for (int k = 0; k < L_count; k++) {
        int col = L_j[k];
        int64_t pos = lu->L_col_ptr[col] + work_ptr[col];
        lu->L_row_idx[pos] = L_i[k];
        lu->L_values[pos] = L_v[k];
        work_ptr[col]++;
    }

    free(work_ptr);
    free(B);
    free(row_count); free(col_count);
    free(row_elim); free(col_elim);
    free(L_i); free(L_j); free(L_v);

    lu->valid = 1;
    return 0;
}
