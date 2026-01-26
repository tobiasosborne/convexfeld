/**
 * @file basis_stub.c
 * @brief Stub implementations for basis operations (M5.1.1 TDD).
 *
 * Provides minimal stubs for EtaFactors, FTRAN, BTRAN, and related
 * functions. BasisState lifecycle in basis_state.c (M5.1.2).
 * Full implementations in M5.1.3-M5.1.8.
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
 * EtaFactors lifecycle - Implemented in eta_factors.c (M5.1.3)
 ******************************************************************************/

/* cxf_eta_create, cxf_eta_free, cxf_eta_init, cxf_eta_validate, cxf_eta_set
 * are implemented in eta_factors.c */

/*******************************************************************************
 * FTRAN/BTRAN - Implementation in M5.1.4 / M5.1.5
 ******************************************************************************/

/* cxf_ftran is implemented in ftran.c (M5.1.4) */
/* cxf_btran is implemented in btran.c (M5.1.5) */

/*******************************************************************************
 * Refactorization - Implemented in refactor.c (M5.1.6)
 ******************************************************************************/

/* cxf_basis_refactor, cxf_solver_refactor, cxf_refactor_check
 * are implemented in refactor.c */

/*******************************************************************************
 * Basis snapshot/comparison - Implementation in M5.1.7
 ******************************************************************************/

/**
 * @brief Create a snapshot of the current basis.
 * @note Full implementation in M5.1.7
 */
int *cxf_basis_snapshot(BasisState *basis) {
    if (basis == NULL || basis->m == 0) {
        return NULL;
    }

    int *snapshot = (int *)malloc((size_t)basis->m * sizeof(int));
    if (snapshot == NULL) {
        return NULL;
    }

    memcpy(snapshot, basis->basic_vars, (size_t)basis->m * sizeof(int));
    return snapshot;
}

/**
 * @brief Compute difference between two basis snapshots.
 * @note Full implementation in M5.1.7
 */
int cxf_basis_diff(const int *snap1, const int *snap2, int m) {
    if (snap1 == NULL || snap2 == NULL) {
        return -1;
    }

    int diff = 0;
    for (int i = 0; i < m; i++) {
        if (snap1[i] != snap2[i]) {
            diff++;
        }
    }
    return diff;
}

/**
 * @brief Check if basis equals a snapshot.
 * @note Full implementation in M5.1.7
 */
int cxf_basis_equal(BasisState *basis, const int *snapshot, int m) {
    if (basis == NULL || snapshot == NULL) {
        return 0;
    }
    if (basis->m != m) {
        return 0;
    }

    for (int i = 0; i < m; i++) {
        if (basis->basic_vars[i] != snapshot[i]) {
            return 0;
        }
    }
    return 1;
}

/*******************************************************************************
 * Validation/warm start - Implemented in warm.c (M5.1.8)
 ******************************************************************************/

/* cxf_basis_validate, cxf_basis_validate_ex, cxf_basis_warm,
 * cxf_basis_warm_snapshot are all implemented in warm.c */
