/**
 * @file snapshot.c
 * @brief BasisSnapshot implementation (M5.1.7)
 *
 * Provides snapshot functionality for capturing, comparing, and restoring
 * basis states. Used for debugging, warm-starting, and iteration tracking.
 *
 * Spec: docs/specs/functions/cxf_basis_snapshot.md
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a snapshot of the current basis state.
 *
 * Captures the complete basis state including:
 * - Dimensions (numVars, numConstrs)
 * - basisHeader array (basic variable indices)
 * - varStatus array (status of all variables)
 * - Current iteration number
 *
 * The includeFactors parameter is reserved for future use to copy
 * L, U factors and pivot permutation when available.
 *
 * @param basis Source basis state.
 * @param snapshot Destination snapshot (caller allocated struct).
 * @param includeFactors If 1, copy factorization data (currently no-op).
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if args are NULL,
 *         CXF_ERROR_OUT_OF_MEMORY on allocation failure.
 */
int cxf_basis_snapshot_create(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors) {
    if (basis == NULL || snapshot == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Initialize snapshot fields */
    snapshot->numVars = basis->n;
    snapshot->numConstrs = basis->m;
    snapshot->iteration = basis->iteration;
    snapshot->valid = 0;
    snapshot->basisHeader = NULL;
    snapshot->varStatus = NULL;
    snapshot->L = NULL;
    snapshot->U = NULL;
    snapshot->pivotPerm = NULL;

    /* Allocate and copy basisHeader if constraints exist */
    if (basis->m > 0) {
        snapshot->basisHeader = (int *)malloc((size_t)basis->m * sizeof(int));
        if (snapshot->basisHeader == NULL) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        memcpy(snapshot->basisHeader, basis->basic_vars,
               (size_t)basis->m * sizeof(int));
    }

    /* Allocate and copy varStatus if variables exist */
    if (basis->n > 0) {
        snapshot->varStatus = (int *)malloc((size_t)basis->n * sizeof(int));
        if (snapshot->varStatus == NULL) {
            free(snapshot->basisHeader);
            snapshot->basisHeader = NULL;
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        memcpy(snapshot->varStatus, basis->var_status,
               (size_t)basis->n * sizeof(int));
    }

    /* Mark snapshot as valid */
    snapshot->valid = 1;

    /* includeFactors: reserved for future L/U copying */
    (void)includeFactors;

    return CXF_OK;
}

/**
 * @brief Compute the number of differences between two snapshots.
 *
 * Compares basisHeader and varStatus arrays element by element.
 * Returns -1 if dimensions do not match or arguments are invalid.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return Number of differing elements, or -1 on error/mismatch.
 */
int cxf_basis_snapshot_diff(const BasisSnapshot *s1, const BasisSnapshot *s2) {
    if (s1 == NULL || s2 == NULL) {
        return -1;
    }
    if (!s1->valid || !s2->valid) {
        return -1;
    }
    if (s1->numVars != s2->numVars || s1->numConstrs != s2->numConstrs) {
        return -1;
    }

    int diff = 0;

    /* Compare basisHeader */
    for (int i = 0; i < s1->numConstrs; i++) {
        if (s1->basisHeader[i] != s2->basisHeader[i]) {
            diff++;
        }
    }

    /* Compare varStatus */
    for (int i = 0; i < s1->numVars; i++) {
        if (s1->varStatus[i] != s2->varStatus[i]) {
            diff++;
        }
    }

    return diff;
}

/**
 * @brief Check if two snapshots are identical.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return 1 if equal, 0 if different or on error.
 */
int cxf_basis_snapshot_equal(const BasisSnapshot *s1, const BasisSnapshot *s2) {
    return cxf_basis_snapshot_diff(s1, s2) == 0;
}

/**
 * @brief Free memory allocated within a snapshot.
 *
 * Frees basisHeader, varStatus, and any factor copies.
 * Sets valid to 0. Does not free the snapshot struct itself.
 * Safe to call with NULL.
 *
 * @param snapshot Snapshot to free (may be NULL).
 */
void cxf_basis_snapshot_free(BasisSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }

    free(snapshot->basisHeader);
    free(snapshot->varStatus);
    free(snapshot->pivotPerm);
    /* L and U are void* - would need type info to free properly */
    /* For now, assume they are NULL or externally managed */

    snapshot->basisHeader = NULL;
    snapshot->varStatus = NULL;
    snapshot->pivotPerm = NULL;
    snapshot->L = NULL;
    snapshot->U = NULL;
    snapshot->valid = 0;
}
