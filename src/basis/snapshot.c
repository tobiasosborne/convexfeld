/**
 * @file snapshot.c
 * @brief Full BasisSnapshot implementation (M5.1.7)
 *
 * HEAVY snapshot: copies entire basisHeader and varStatus arrays.
 * Used for warm-starting, debugging, and basis comparison.
 *
 * This is an IMPLEMENTATION EXTENSION — NOT the V2 spec snapshot.
 * The V2 spec's cxf_basis_snapshot (lightweight O(1) counter snapshot)
 * lives in basis_stub.c as cxf_progress_snapshot().
 *
 * Two snapshot mechanisms exist:
 *   1. Lightweight (spec): cxf_progress_snapshot() in basis_stub.c
 *      — copies CXF_SNAPSHOT_SIZE scalar counters, O(1)
 *   2. Heavy (this file): cxf_basis_snapshot_full()
 *      — copies full basisHeader + varStatus arrays, O(m+n)
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a FULL snapshot of the current basis state (heavy).
 *
 * This is the heavy snapshot — copies entire basisHeader and varStatus
 * arrays. For the lightweight V2 spec counter snapshot, see
 * cxf_progress_snapshot() in basis_stub.c.
 *
 * @param basis Source basis state.
 * @param snapshot Destination snapshot (caller allocated struct).
 * @param includeFactors If 1, copy factorization data (currently no-op).
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT if args are NULL,
 *         CXF_ERROR_OUT_OF_MEMORY on allocation failure.
 */
int cxf_basis_snapshot_full(BasisState *basis, BasisSnapshot *snapshot,
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
 * @brief Count element-wise differences between two full snapshots (heavy).
 *
 * Returns an integer COUNT of differing basisHeader + varStatus elements.
 * This is NOT the V2 spec's cxf_basis_diff (weighted double score).
 * For the spec-compliant weighted diff, see cxf_basis_diff() in basis_stub.c.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return Number of differing elements, or -1 on error/mismatch.
 */
int cxf_basis_snapshot_full_diff(const BasisSnapshot *s1, const BasisSnapshot *s2) {
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
 * @brief Check if two full basis snapshots are identical.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return 1 if equal, 0 if different or on error.
 */
int cxf_basis_snapshot_full_equal(const BasisSnapshot *s1, const BasisSnapshot *s2) {
    return cxf_basis_snapshot_full_diff(s1, s2) == 0;
}

/**
 * @brief Free memory allocated within a full basis snapshot.
 *
 * Frees basisHeader, varStatus, and any factor copies.
 * Sets valid to 0. Does not free the snapshot struct itself.
 * Safe to call with NULL.
 *
 * @param snapshot Snapshot to free (may be NULL).
 */
void cxf_basis_snapshot_full_free(BasisSnapshot *snapshot) {
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
