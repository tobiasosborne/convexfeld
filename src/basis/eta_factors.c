/**
 * @file eta_factors.c
 * @brief EtaVector structure implementation (M5.1.3)
 *
 * Implements lifecycle and utility functions for EtaVector structure.
 * EtaVector represents elementary transformation matrices in the
 * Product Form of Inverse (PFI) representation of the basis inverse.
 */

#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*******************************************************************************
 * EtaVector lifecycle
 ******************************************************************************/

/**
 * @brief Create an EtaVector structure.
 *
 * Allocates an eta matrix with space for the specified number of non-zeros.
 * The eta represents an elementary transformation matrix that differs from
 * the identity in only one column/row.
 *
 * @param type Eta type: CXF_ETA_PIVOT, CXF_ETA_VARIABLE_FIX, CXF_ETA_WARM_START.
 * @param pivot_row Row index for the pivot operation.
 * @param nnz Number of non-zeros in the sparse representation.
 * @return Pointer to new EtaVector, or NULL on failure.
 */
EtaVector *cxf_eta_create(int type, int pivot_row, int nnz) {
    if (nnz < 0) {
        return NULL;
    }

    EtaVector *eta = (EtaVector *)calloc(1, sizeof(EtaVector));
    if (eta == NULL) {
        return NULL;
    }

    eta->type = type;
    eta->pivot_row = pivot_row;
    eta->nnz = nnz;
    eta->pivot_elem = 1.0;
    eta->next = NULL;

    if (nnz > 0) {
        eta->indices = (int *)calloc((size_t)nnz, sizeof(int));
        eta->values = (double *)calloc((size_t)nnz, sizeof(double));
        if (eta->indices == NULL || eta->values == NULL) {
            free(eta->indices);
            free(eta->values);
            free(eta);
            return NULL;
        }
    }

    return eta;
}

/**
 * @brief Free an EtaVector structure.
 *
 * Frees the eta and all associated arrays. Safe to call with NULL.
 *
 * @param eta EtaVector to free (may be NULL).
 */
void cxf_eta_free(EtaVector *eta) {
    if (eta == NULL) {
        return;
    }
    free(eta->indices);
    free(eta->values);
    free(eta);
}

/**
 * @brief Initialize/reinitialize an existing EtaVector.
 *
 * Resizes arrays if necessary. Clears all values to zero.
 *
 * @param eta EtaVector to initialize.
 * @param type Eta type: CXF_ETA_PIVOT, CXF_ETA_VARIABLE_FIX, CXF_ETA_WARM_START.
 * @param pivot_row Row index for pivot.
 * @param nnz Number of non-zeros (must match allocated capacity).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_eta_init(EtaVector *eta, int type, int pivot_row, int nnz) {
    if (eta == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (nnz < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Free existing arrays if size changed */
    if (eta->nnz != nnz) {
        free(eta->indices);
        free(eta->values);
        eta->indices = NULL;
        eta->values = NULL;

        if (nnz > 0) {
            eta->indices = (int *)calloc((size_t)nnz, sizeof(int));
            eta->values = (double *)calloc((size_t)nnz, sizeof(double));
            if (eta->indices == NULL || eta->values == NULL) {
                free(eta->indices);
                free(eta->values);
                eta->indices = NULL;
                eta->values = NULL;
                eta->nnz = 0;
                return CXF_ERROR_OUT_OF_MEMORY;
            }
        }
    } else if (nnz > 0) {
        /* Same size, just zero out */
        memset(eta->indices, 0, (size_t)nnz * sizeof(int));
        memset(eta->values, 0, (size_t)nnz * sizeof(double));
    }

    eta->type = type;
    eta->pivot_row = pivot_row;
    eta->nnz = nnz;
    eta->pivot_elem = 1.0;
    /* Note: next pointer unchanged - caller manages list */

    return CXF_OK;
}

/**
 * @brief Validate EtaVector invariants.
 *
 * Checks that the eta structure is consistent and all values are finite.
 *
 * @param eta EtaVector to validate.
 * @param max_rows Maximum valid row index (exclusive).
 * @return CXF_OK if valid, error code otherwise.
 */
int cxf_eta_validate(const EtaVector *eta, int max_rows) {
    if (eta == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check nnz is non-negative */
    if (eta->nnz < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Check type is valid (V2: PIVOT, VARIABLE_FIX, or WARM_START) */
    if (eta->type != CXF_ETA_PIVOT &&
        eta->type != CXF_ETA_VARIABLE_FIX &&
        eta->type != CXF_ETA_WARM_START) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Check pivot_row is in range */
    if (eta->pivot_row < 0 || eta->pivot_row >= max_rows) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Check pivot_elem is finite and non-zero */
    if (!isfinite(eta->pivot_elem) || eta->pivot_elem == 0.0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Check arrays exist if nnz > 0 */
    if (eta->nnz > 0) {
        if (eta->indices == NULL || eta->values == NULL) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }

        /* Check indices are in range and values are finite */
        for (int i = 0; i < eta->nnz; i++) {
            if (eta->indices[i] < 0 || eta->indices[i] >= max_rows) {
                return CXF_ERROR_INVALID_ARGUMENT;
            }
            if (!isfinite(eta->values[i])) {
                return CXF_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    return CXF_OK;
}

/**
 * @brief Set values in EtaVector arrays.
 *
 * Copies indices and values into the eta structure.
 *
 * @param eta EtaVector to modify.
 * @param indices Array of row indices (length = eta->nnz).
 * @param values Array of coefficient values (length = eta->nnz).
 * @return CXF_OK on success, error code on failure.
 */
int cxf_eta_set(EtaVector *eta, const int *indices, const double *values) {
    if (eta == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (eta->nnz > 0 && (indices == NULL || values == NULL)) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (eta->nnz > 0 && (eta->indices == NULL || eta->values == NULL)) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    if (eta->nnz > 0) {
        memcpy(eta->indices, indices, (size_t)eta->nnz * sizeof(int));
        memcpy(eta->values, values, (size_t)eta->nnz * sizeof(double));
    }

    return CXF_OK;
}
