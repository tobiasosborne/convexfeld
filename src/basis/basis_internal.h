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

/* --- Basis lifecycle (basis_state.c) --- */
struct BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(struct BasisState *basis);

/* --- FTRAN/BTRAN (ftran.c, btran.c) --- */
int cxf_ftran(struct BasisState *basis, const double *column, double *result);
int cxf_btran(struct BasisState *basis, int row, double *result);

/* --- Pivot (pivot_eta.c) --- */
int cxf_pivot_with_eta(struct BasisState *basis, int pivotRow,
                       const double *pivotCol, int enteringVar,
                       int leavingVar, int leavingStatus);

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

#endif /* BASIS_INTERNAL_H */
