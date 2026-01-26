/**
 * @file basis_state.c
 * @brief BasisState structure implementation (M5.1.2)
 *
 * Implements lifecycle functions for the BasisState structure that
 * maintains the simplex basis using Product Form of Inverse (PFI).
 *
 * Spec: docs/specs/structures/basis_state.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/* Default refactorization frequency (pivots between refactorizations) */
#define DEFAULT_REFACTOR_FREQ 100

/**
 * @brief Create and initialize a BasisState structure.
 *
 * Allocates a new BasisState for a problem with m constraints
 * and n variables. The basis is initially empty (no basic variables
 * assigned) with eta count zero.
 *
 * @param m Number of constraints (= number of basic variables).
 * @param n Number of variables.
 * @return Pointer to new BasisState, or NULL on allocation failure.
 *
 * @note Caller owns the returned BasisState and must call cxf_basis_free().
 */
BasisState *cxf_basis_create(int m, int n) {
    if (m < 0 || n < 0) {
        return NULL;
    }

    BasisState *basis = (BasisState *)calloc(1, sizeof(BasisState));
    if (basis == NULL) {
        return NULL;
    }

    basis->m = m;
    basis->eta_count = 0;
    basis->eta_capacity = 0;
    basis->eta_head = NULL;
    basis->pivots_since_refactor = 0;
    basis->refactor_freq = DEFAULT_REFACTOR_FREQ;

    /* Allocate arrays for constraints (rows) */
    if (m > 0) {
        basis->basic_vars = (int *)calloc((size_t)m, sizeof(int));
        basis->work = (double *)calloc((size_t)m, sizeof(double));
        if (basis->basic_vars == NULL || basis->work == NULL) {
            free(basis->basic_vars);
            free(basis->work);
            free(basis);
            return NULL;
        }
    }

    /* Allocate variable status array */
    if (n > 0) {
        basis->var_status = (int *)calloc((size_t)n, sizeof(int));
        if (basis->var_status == NULL) {
            free(basis->basic_vars);
            free(basis->work);
            free(basis);
            return NULL;
        }
    }

    return basis;
}

/**
 * @brief Free a BasisState and all associated memory.
 *
 * Deallocates the BasisState structure including all arrays and
 * the entire eta vector linked list. Safe to call with NULL.
 *
 * @param basis BasisState to free (may be NULL).
 */
void cxf_basis_free(BasisState *basis) {
    if (basis == NULL) {
        return;
    }

    /* Free eta linked list */
    EtaFactors *eta = basis->eta_head;
    while (eta != NULL) {
        EtaFactors *next = eta->next;
        free(eta->indices);
        free(eta->values);
        free(eta);
        eta = next;
    }

    /* Free arrays */
    free(basis->basic_vars);
    free(basis->var_status);
    free(basis->work);
    free(basis);
}

/**
 * @brief Initialize or reinitialize a BasisState with given dimensions.
 *
 * This function validates the basis state and optionally reinitializes
 * it for new problem dimensions. Used for warm-starting or resetting.
 *
 * @param basis BasisState to initialize (must be non-NULL).
 * @param m Number of constraints.
 * @param n Number of variables.
 * @return CXF_OK on success, error code on failure.
 */
int cxf_basis_init(BasisState *basis, int m, int n) {
    if (basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (m < 0 || n < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Validate dimensions match existing allocation */
    if (m != basis->m) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Reset eta list state */
    basis->eta_count = 0;
    basis->eta_head = NULL;
    basis->pivots_since_refactor = 0;

    /* Clear arrays */
    if (basis->basic_vars != NULL && m > 0) {
        memset(basis->basic_vars, 0, (size_t)m * sizeof(int));
    }
    if (basis->var_status != NULL && n > 0) {
        memset(basis->var_status, 0, (size_t)n * sizeof(int));
    }
    if (basis->work != NULL && m > 0) {
        memset(basis->work, 0, (size_t)m * sizeof(double));
    }

    return CXF_OK;
}
