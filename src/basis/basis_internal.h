/**
 * @file basis_internal.h
 * @brief Internal declarations for cross-file basis module functions.
 *
 * Replaces scattered extern declarations with a single shared header.
 */

#ifndef BASIS_INTERNAL_H
#define BASIS_INTERNAL_H

#include <stddef.h>

/* Forward declarations */
struct BasisState;
struct SolverState;
struct EtaBuffer;
struct LUFactors;

/* --- Basis lifecycle (basis_state.c) --- */
struct BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(struct BasisState *basis);

/* --- FTRAN/BTRAN (ftran.c, btran.c, btran_etas.c) --- */
int cxf_ftran(struct BasisState *basis, const double *column, double *result);
int cxf_btran(struct BasisState *basis, int row, double *result);

/* Shared BTRAN helpers (btran_etas.c) */
int btran_apply_lu(const struct LUFactors *lu, int m, double *result,
                   double *temp);
void btran_apply_diag(const double *diag_coeff, int m, double *result);
int btran_apply_etas(struct BasisState *basis, int m, double *result);

/* Shared eta collection (btran_etas.c) — used by FTRAN and BTRAN */

/** Maximum stack-allocated eta pointers before heap allocation */
#define ETA_MAX_STACK 64

/**
 * @brief Collect eta pointers from linked list into an array.
 *
 * Uses stack buffer for small counts, heap for large.  Caller must
 * call eta_collect_free() with the same stack_buf when done.
 *
 * @param basis     BasisState with eta linked list.
 * @param stack_buf Caller-provided stack buffer of size ETA_MAX_STACK.
 * @param out_etas  Set to array of eta pointers (stack or heap).
 * @param out_count Set to number of etas collected.
 * @return CXF_OK or CXF_ERROR_OUT_OF_MEMORY.
 */
int eta_collect(struct BasisState *basis,
                struct EtaVector **stack_buf,
                struct EtaVector ***out_etas,
                int *out_count);

/** @brief Validate a single eta vector (bounds + stability). */
int eta_validate(const struct EtaVector *eta, int m);

/** @brief Free heap-allocated eta array (no-op if stack). */
void eta_collect_free(struct EtaVector **etas,
                      struct EtaVector **stack_buf);

/* --- Pivot (pivot_eta.c) --- */
int cxf_pivot_with_eta(struct BasisState *basis, int pivotRow,
                       const double *pivotCol, int enteringVar,
                       int leavingVar, int leavingStatus,
                       double reducedCost, int direction);

/* --- Refactorization (refactor.c) --- */
int cxf_solver_refactor(struct SolverState *ctx, struct CxfEnv *env);
int cxf_refactor_check(struct SolverState *state, struct CxfEnv *env);
int cxf_fix_variables_at_bounds(struct BasisState *basis);

/* --- Eta pool (eta_pool.c) --- */
struct EtaBuffer *cxf_eta_pool_create(size_t initial_size);
void *cxf_eta_pool_alloc(struct EtaBuffer *pool, size_t size);
void cxf_eta_pool_free(struct EtaBuffer *pool);
void cxf_eta_list_clear(struct BasisState *basis);

/* --- Snapshot/diff (basis_stub.c) --- */
void cxf_progress_snapshot(struct SolverState *state);
double cxf_basis_diff(struct SolverState *state);

/* --- Sparse LU working matrix (sparse_work.c, sparse_elim.c) --- */

/** Per-column sparse vector for LU working matrix. */
typedef struct {
    int *row_idx;    /**< Row indices of nonzero entries */
    double *values;  /**< Corresponding values */
    int len;         /**< Current number of entries */
    int cap;         /**< Allocated capacity */
} SparseCol;

/** Sparse working matrix for Markowitz-ordered LU factorization.
 *  Suhl & Suhl (1990), Maros (2003) Ch. 5. */
typedef struct {
    int m;               /**< Matrix dimension */
    SparseCol *cols;     /**< [m] column vectors */
    int *row_count;      /**< [m] nonzeros per active row */
    double *col_max;     /**< [m] column maxima for threshold pivoting */
    int *row_active;     /**< [m] 1=active, 0=eliminated */
    int *col_active;     /**< [m] 1=active, 0=eliminated */
    int active_count;    /**< Remaining active rows/cols */
    int total_nnz;       /**< Current total nonzeros in active submatrix */
} SparseWork;

/* sparse_work.c */
SparseWork *sparse_work_create(int m);
int sparse_work_extract(SparseWork *sw, struct SolverState *ctx);
void sparse_work_free(SparseWork *sw);
int sparse_col_append(SparseCol *col, int row, double val);
double sparse_work_density(const SparseWork *sw);
int sparse_extract_pivot_row(const SparseWork *sw, int piv_row,
                             int *cols, double *vals);
int sparse_to_dense(const SparseWork *sw,
                    double *D, int *map_row, int *map_col);

/* sparse_elim.c */
int sparse_find_pivot(const SparseWork *sw,
                      int *out_row, int *out_col, double *out_val);
int sparse_eliminate(SparseWork *sw, int piv_row, int piv_col,
                     double piv_val,
                     const int *pr_cols, const double *pr_vals, int pr_len,
                     int **L_i, int **L_j, double **L_v,
                     int *L_count, int *L_cap, int step);

/* dense_elim.c */
int dense_find_pivot(const double *D, int n,
                     const int *relim, const int *celim,
                     int *out_r, int *out_c, double *out_v);
int dense_eliminate(double *D, int n, int piv_r, int piv_c,
                    double piv_val, int *relim, int *celim,
                    int **L_i, int **L_j, double **L_v,
                    int *Lc, int *Lcap, int step);

/* lu_output.c */
int build_lu_output(struct LUFactors *lu, int m,
                    const int *Li, const int *Lj, const double *Lv,
                    int Lc,
                    const int *Ui, const int *Uj, const double *Uv,
                    int Uc);

#endif /* BASIS_INTERNAL_H */
