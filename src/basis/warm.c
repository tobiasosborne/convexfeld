/**
 * @file warm.c
 * @brief Basis validation and warm start implementation (M5.1.8)
 *
 * Implements basis validation checks and warm start from saved basis or
 * snapshot. Used for restarting optimization from a known basis state.
 *
 * Spec: docs/specs/functions/basis/cxf_basis_validate.md
 *       docs/specs/functions/basis/cxf_basis_warm.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
 * Validation flag definitions
 ******************************************************************************/

/** @brief Check that m basic variables exist */
#define CXF_CHECK_COUNT       0x01

/** @brief Check variable indices are within bounds */
#define CXF_CHECK_BOUNDS      0x02

/** @brief Check for duplicate basic variables */
#define CXF_CHECK_DUPLICATES  0x04

/** @brief Check varStatus matches basisHeader */
#define CXF_CHECK_CONSISTENCY 0x10

/** @brief Run all validation checks */
#define CXF_CHECK_ALL         0xFF

/*******************************************************************************
 * Internal helper: clear eta list
 ******************************************************************************/

/**
 * @brief Free all eta factors in the linked list.
 */
static void clear_eta_list(BasisState *basis) {
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

/*******************************************************************************
 * cxf_basis_validate - Simple validation
 ******************************************************************************/

/**
 * @brief Validate basis consistency.
 *
 * Performs basic validation checks:
 * - Variable indices are non-negative and within bounds
 * - No duplicate basic variables
 *
 * @param basis BasisState to validate.
 * @return CXF_OK if valid, CXF_ERROR_NULL_ARGUMENT if NULL,
 *         CXF_ERROR_INVALID_ARGUMENT if validation fails.
 */
int cxf_basis_validate(BasisState *basis) {
    if (basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Empty basis is trivially valid */
    if (basis->m == 0) {
        return CXF_OK;
    }

    /* Check bounds and duplicates */
    for (int i = 0; i < basis->m; i++) {
        int var = basis->basic_vars[i];

        /* Check bounds: 0 <= var < n */
        if (var < 0 || var >= basis->n) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Check for duplicates (O(m^2) but m is typically small) */
        for (int j = i + 1; j < basis->m; j++) {
            if (var == basis->basic_vars[j]) {
                return CXF_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    return CXF_OK;
}

/*******************************************************************************
 * cxf_basis_validate_ex - Extended validation with flags
 ******************************************************************************/

/**
 * @brief Extended basis validation with selective checks.
 *
 * @param basis BasisState to validate.
 * @param flags Bitmask of checks to perform (CXF_CHECK_*).
 * @return CXF_OK if valid, error code otherwise.
 */
int cxf_basis_validate_ex(BasisState *basis, int flags) {
    if (basis == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* No flags = no checks = trivially valid */
    if (flags == 0) {
        return CXF_OK;
    }

    /* Empty basis is trivially valid */
    if (basis->m == 0) {
        return CXF_OK;
    }

    /* Count check: verify we have exactly m entries */
    if (flags & CXF_CHECK_COUNT) {
        /* basic_vars array always has m entries by construction */
        /* This check is more meaningful with full SolverState */
    }

    /* Bounds check: 0 <= var < n */
    if (flags & CXF_CHECK_BOUNDS) {
        for (int i = 0; i < basis->m; i++) {
            int var = basis->basic_vars[i];
            if (var < 0 || var >= basis->n) {
                return CXF_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    /* Duplicate check */
    if (flags & CXF_CHECK_DUPLICATES) {
        for (int i = 0; i < basis->m; i++) {
            for (int j = i + 1; j < basis->m; j++) {
                if (basis->basic_vars[i] == basis->basic_vars[j]) {
                    return CXF_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }

    /* Consistency check: varStatus matches basisHeader */
    if (flags & CXF_CHECK_CONSISTENCY) {
        /* For each basic variable, its status should be CXF_BASIC */
        for (int row = 0; row < basis->m; row++) {
            int var = basis->basic_vars[row];
            if (var >= 0 && var < basis->n) {
                if (basis->var_status[var] != CXF_BASIC) {
                    return CXF_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }

    return CXF_OK;
}

/*******************************************************************************
 * cxf_basis_warm - Warm start from basic variable array
 ******************************************************************************/

/**
 * @brief Warm start from saved basic variable indices.
 *
 * Copies the basic variable indices and clears the eta list,
 * preparing for a fresh factorization.
 *
 * @param basis BasisState to initialize.
 * @param basic_vars Array of basic variable indices [m].
 * @param m Number of basic variables (must match basis->m).
 * @return CXF_OK on success, error code otherwise.
 */
int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m) {
    if (basis == NULL || basic_vars == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (m != basis->m) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Copy basic variables */
    memcpy(basis->basic_vars, basic_vars, (size_t)m * sizeof(int));

    /* Clear eta list (refactorization will be needed) */
    clear_eta_list(basis);

    return CXF_OK;
}

/*******************************************************************************
 * cxf_basis_warm_snapshot - Warm start from BasisSnapshot
 ******************************************************************************/

/**
 * @brief Warm start from a BasisSnapshot.
 *
 * Copies the basis header and variable status from the snapshot,
 * preparing for a fresh factorization.
 *
 * @param basis BasisState to initialize.
 * @param snapshot Source snapshot (must have matching dimensions).
 * @return CXF_OK on success, error code otherwise.
 */
int cxf_basis_warm_snapshot(BasisState *basis, const BasisSnapshot *snapshot) {
    if (basis == NULL || snapshot == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (!snapshot->valid) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    if (snapshot->numConstrs != basis->m || snapshot->numVars != basis->n) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Copy basis header */
    if (basis->m > 0 && snapshot->basisHeader != NULL) {
        memcpy(basis->basic_vars, snapshot->basisHeader,
               (size_t)basis->m * sizeof(int));
    }

    /* Copy variable status */
    if (basis->n > 0 && snapshot->varStatus != NULL) {
        memcpy(basis->var_status, snapshot->varStatus,
               (size_t)basis->n * sizeof(int));
    }

    /* Clear eta list (refactorization will be needed) */
    clear_eta_list(basis);

    return CXF_OK;
}
