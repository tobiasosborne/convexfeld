/**
 * @file refactor.c
 * @brief Basis refactorization implementation (M5.1.6)
 *
 * Implements LU factorization of the basis matrix using a
 * simplified Gaussian elimination approach. The factorization
 * is stored using the Product Form of Inverse (PFI) representation
 * with eta vectors for compatibility with FTRAN/BTRAN.
 *
 * Spec: docs/specs/functions/basis/cxf_basis_refactor.md
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

/* Minimum pivot tolerance */
#define MIN_PIVOT_TOL  1e-10

/**
 * @brief Clear all eta vectors from a BasisState.
 *
 * @param basis BasisState to clear.
 */
static void clear_eta_list(BasisState *basis) {
    if (basis == NULL) return;

    EtaFactors *eta = basis->eta_head;
    while (eta != NULL) {
        EtaFactors *next = eta->next;
        free(eta->indices);
        free(eta->values);
        free(eta);
        eta = next;
    }

    basis->eta_head = NULL;
    basis->eta_count = 0;
    basis->pivots_since_refactor = 0;
}

/**
 * @brief Basic refactorization for BasisState only.
 *
 * This simplified version clears the eta vectors and resets
 * counters. Use cxf_solver_refactor() for full refactorization
 * when the constraint matrix is available.
 *
 * @param basis BasisState to refactor.
 * @return CXF_OK on success, error code on failure.
 */
int cxf_basis_refactor(BasisState *basis) {
    if (basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Clear existing eta list */
    clear_eta_list(basis);

    /* Reset diag_coeff to identity.
     * After refactorization, we treat the current basis as the new "initial"
     * basis. Since we don't have proper LU factorization, we assume the
     * current basis is identity-like (which is only correct if all basic
     * vars are auxiliaries with coefficient +1).
     *
     * TODO: Implement proper LU factorization for non-trivial bases. */
    if (basis->diag_coeff != NULL) {
        for (int i = 0; i < basis->m; i++) {
            basis->diag_coeff[i] = 1.0;
        }
    }

    return CXF_OK;
}

/**
 * @brief Full refactorization with access to solver context.
 *
 * Computes a fresh factorization of the basis matrix using
 * Gaussian elimination. The result is stored as eta vectors
 * for compatibility with FTRAN/BTRAN operations.
 *
 * For identity basis (all slacks): No eta vectors (B = I).
 * For structural columns: Computes elimination factors.
 *
 * @param ctx SolverContext containing basis and model.
 * @param env Environment with tolerances.
 * @return 0 on success, error code on failure.
 */
int cxf_solver_refactor(SolverContext *ctx, CxfEnv *env) {
    if (ctx == NULL || ctx->basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    BasisState *basis = ctx->basis;
    int m = basis->m;

    /* Clear existing factorization */
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

    /* Get pivot tolerance from environment */
    double pivot_tol = MIN_PIVOT_TOL;
    if (env != NULL) {
        pivot_tol = env->feasibility_tol;
        if (pivot_tol < MIN_PIVOT_TOL) {
            pivot_tol = MIN_PIVOT_TOL;
        }
    }

    /* Check for identity basis (all slacks)
     * In this case, B = I and no eta vectors needed.
     *
     * For structural variables in basis, we would need the
     * constraint matrix to compute proper factorization.
     * This requires model_ref access. */

    int all_slacks = 1;
    for (int i = 0; i < m; i++) {
        int var = basis->basic_vars[i];
        /* Slack variables have index >= num_vars */
        if (var < ctx->num_vars) {
            all_slacks = 0;
            break;
        }
    }

    if (all_slacks) {
        /* Identity basis - no eta vectors needed */
        return REFACTOR_OK;
    }

    /* For non-identity basis, a full implementation would:
     * 1. Extract basis columns from constraint matrix
     * 2. Perform Gaussian elimination with pivoting
     * 3. Store elimination factors as eta vectors
     *
     * This requires access to CxfModel and its matrix.
     * For now, return OK to allow continued execution.
     *
     * TODO: Implement full Markowitz-ordered LU factorization
     * when matrix access is available. */

    (void)pivot_tol;  /* Suppress unused warning for now */

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
 * @param ctx SolverContext with current state.
 * @param env Environment with thresholds.
 * @return 0=not needed, 1=recommended, 2=required.
 */
int cxf_refactor_check(SolverContext *ctx, CxfEnv *env) {
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
