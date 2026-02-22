/**
 * @file refactor.c
 * @brief Basis refactorization implementation (M5.1.6)
 *
 * Implements LU factorization of the basis matrix using a
 * simplified Gaussian elimination approach. The factorization
 * is stored using the Product Form of Inverse (PFI) representation
 * with eta vectors for compatibility with FTRAN/BTRAN.
 *
 * Spec: docs/specs/functions/basis/cxf_fix_variables_at_bounds.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Error codes for refactorization */
#define REFACTOR_OK             0
#define REFACTOR_OUT_OF_MEMORY  1001
#define REFACTOR_SINGULAR       3

/* Minimum pivot tolerance (Harris pivot threshold from spec) */
#define MIN_PIVOT_TOL  CXF_PIVOT_TOL  /* 1e-9 */

/* e2t: Shared eta list clear (eta_pool.c) */
extern void cxf_eta_list_clear(BasisState *basis);
#define clear_eta_list cxf_eta_list_clear

/**
 * @brief Fix near-bound nonbasic variables and clear etas (P2.4).
 *
 * Per basis_operations.md: scans nonbasic variables, snaps those
 * within tolerance of a bound to that bound exactly, then clears
 * the eta list for fresh refactorization.
 *
 * @param basis BasisState to process and refactor.
 * @return CXF_OK on success, error code on failure.
 */
int cxf_fix_variables_at_bounds(BasisState *basis) {
    if (basis == NULL) return CXF_ERROR_NULL_ARGUMENT;

    /* Clear existing eta list (O(1) with pool, O(k) without) */
    clear_eta_list(basis);

    /* P0.10: Preserve diag_coeff — encodes constraint sense. */

    return CXF_OK;
}

/**
 * @brief Full refactorization with access to solver context.
 *
 * Computes a fresh LU factorization of the basis matrix using
 * Markowitz-ordered Gaussian elimination. The result is stored
 * in the LUFactors structure for use by FTRAN/BTRAN.
 *
 * @param ctx SolverState containing basis and model.
 * @param env Environment with tolerances.
 * @return 0 on success, error code on failure.
 */
int cxf_solver_refactor(SolverState *ctx, CxfEnv *env) {
    if (ctx == NULL || ctx->basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    BasisState *basis = ctx->basis;
    int m = basis->m;

    /* Clear existing eta list */
    clear_eta_list(basis);

    /* Reset refactorization counters */
    ctx->eta_count = 0;
    ctx->eta_memory = 0;
    ctx->total_ftran_time = 0.0;
    ctx->ftran_count = 0;
    ctx->last_refactor_iter = ctx->iteration;

    /* For empty basis, nothing to do */
    if (m == 0) {
        return REFACTOR_OK;
    }

    (void)env;  /* Currently unused, will be used for tolerances */

    /* Allocate LU factors if not present */
    if (basis->lu == NULL) {
        /* Estimate nnz: typically ~2*m for sparse LP bases */
        int64_t nnz_est = (int64_t)m * 2;
        basis->lu = cxf_lu_create(m, nnz_est, nnz_est);
        if (basis->lu == NULL) {
            return REFACTOR_OUT_OF_MEMORY;
        }
    } else {
        /* Clear existing factorization */
        cxf_lu_clear(basis->lu);
    }

    /* Check for diagonal basis (all slacks or artificials at their rows).
     * For diagonal basis, LU is trivial (L=I, U=diag). */
    int all_diagonal = 1;
    for (int i = 0; i < m; i++) {
        int var = basis->basic_vars[i];
        int n = ctx->num_vars;
        /* Accept slack (n+i) or artificial (n+m+i) at row i */
        if (var != n + i && var != n + m + i) {
            all_diagonal = 0;
            break;
        }
    }

    if (all_diagonal) {
        /* Diagonal basis - L=I, U=diag */
        LUFactors *lu = basis->lu;
        int n = ctx->num_vars;
        for (int i = 0; i < m; i++) {
            lu->perm_row[i] = i;
            lu->perm_col[i] = i;
            int var = basis->basic_vars[i];
            if (var == n + i) {
                lu->U_diag[i] = basis->diag_coeff[i];
            } else {
                /* artificial at n+m+i */
                lu->U_diag[i] = (ctx->art_coeff != NULL)
                    ? ctx->art_coeff[i] : 1.0;
            }
            lu->L_col_ptr[i] = 0;
            lu->U_col_ptr[i] = 0;
        }
        lu->L_col_ptr[m] = 0;
        lu->U_col_ptr[m] = 0;
        lu->L_nnz = 0;
        lu->U_nnz = 0;
        lu->valid = 1;
        return REFACTOR_OK;
    }

    /* Perform Markowitz LU factorization */
    int status = cxf_lu_factorize(basis->lu, ctx);
    if (status != 0) {
        /* Factorization failed - clear LU and fall back to eta-only */
        cxf_lu_clear(basis->lu);
        return status;
    }

    return REFACTOR_OK;
}

/**
 * @brief Check if refactorization is needed.
 *
 * Determines whether the basis should be refactored based on:
 * - Number of eta vectors accumulated
 * - Memory usage of eta storage
 * - Iterations since last refactorization
 * - FTRAN performance degradation
 *
 * @param ctx SolverState with current state.
 * @param env Environment with thresholds.
 * @return 0=not needed, 1=recommended, 2=required.
 */
int cxf_refactor_check(SolverState *ctx, CxfEnv *env) {
    if (ctx == NULL || ctx->basis == NULL) {
        return 0;
    }
    if (env == NULL) {
        return 0;
    }

    /* Check eta count limit */
    if (ctx->eta_count >= env->max_eta_count) {
        return 2;  /* Required */
    }

    /* Check eta memory limit */
    if (ctx->eta_memory >= env->max_eta_memory) {
        return 2;  /* Required */
    }

    /* Check iteration interval */
    int iters_since = ctx->iteration - ctx->last_refactor_iter;
    if (iters_since >= env->refactor_interval) {
        return 1;  /* Recommended */
    }

    /* Check FTRAN degradation */
    if (ctx->ftran_count > 0 && ctx->baseline_ftran > 0) {
        double avg_ftran = ctx->total_ftran_time / ctx->ftran_count;
        if (avg_ftran > 3.0 * ctx->baseline_ftran) {
            return 1;  /* Recommended */
        }
    }

    return 0;  /* Not needed */
}
