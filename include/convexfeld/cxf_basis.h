/**
 * @file cxf_basis.h
 * @brief BasisState and EtaFactors structures - basis representation.
 *
 * Maintains the simplex basis using Product Form of Inverse (PFI).
 * The basis inverse is represented as a product of eta matrices.
 */

#ifndef CXF_BASIS_H
#define CXF_BASIS_H

#include "cxf_types.h"

/**
 * @brief Eta factors for basis updates.
 *
 * Represents a single elementary transformation matrix.
 * Eta matrices form a linked list for PFI representation.
 */
struct EtaFactors {
    int type;                 /**< 1=refactorization, 2=pivot */
    int pivot_row;            /**< Row index for pivot */
    int pivot_var;            /**< Variable index involved in transformation */
    int nnz;                  /**< Non-zeros in eta vector */
    int *indices;             /**< Row indices [nnz] */
    double *values;           /**< Values [nnz] */
    double pivot_elem;        /**< Pivot element */
    double obj_coeff;         /**< Objective coefficient of pivot_var */
    int status;               /**< New status of pivot_var: -1=lower, -2=upper, -3=superbasic, >=0=basic */
    EtaFactors *next;         /**< Link to next eta (newer) */
};

/**
 * @brief Basis state for simplex method.
 *
 * Tracks which variables are basic and maintains the
 * basis factorization using eta vectors.
 */
struct BasisState {
    int m;                    /**< Number of basic variables (= num_constrs) */
    int n;                    /**< Number of variables */
    int *basic_vars;          /**< Indices of basic variables [m] */
    int *var_status;          /**< Status of each variable [n] */

    /* Eta factorization */
    int eta_count;            /**< Number of eta vectors */
    int eta_capacity;         /**< Capacity for eta vectors */
    EtaFactors *eta_head;     /**< Head of eta linked list */

    /* Working storage */
    double *work;             /**< Working array [m] */

    /* Refactorization control */
    int refactor_freq;        /**< Refactorization frequency */
    int pivots_since_refactor;/**< Pivots since last refactor */
    int iteration;            /**< Current iteration number */
};

/**
 * @brief Snapshot of basis state for comparison and restoration.
 *
 * Captures the complete basis state at a point in time for
 * debugging, comparison, or warm-starting purposes.
 */
typedef struct BasisSnapshot {
    int numVars;              /**< Number of variables */
    int numConstrs;           /**< Number of constraints */
    int *basisHeader;         /**< Basic variable indices [numConstrs] */
    int *varStatus;           /**< Variable status array [numVars + numConstrs] */
    int valid;                /**< 1 if snapshot is valid, 0 otherwise */
    int iteration;            /**< Iteration number when snapshot taken */
    /* Optional factor copies (may be NULL) */
    void *L;                  /**< L factor copy */
    void *U;                  /**< U factor copy */
    int *pivotPerm;           /**< Pivot permutation array */
} BasisSnapshot;

/*******************************************************************************
 * BasisSnapshot functions (M5.1.7)
 ******************************************************************************/

/**
 * @brief Create a snapshot of the current basis state.
 *
 * @param basis Source basis state.
 * @param snapshot Destination snapshot (caller allocated).
 * @param includeFactors If 1, copy factorization data (currently no-op).
 * @return CXF_OK on success, CXF_ERROR_OUT_OF_MEMORY on allocation failure.
 */
int cxf_basis_snapshot_create(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);

/**
 * @brief Compute the number of differences between two snapshots.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return Number of differing elements, or -1 on error.
 */
int cxf_basis_snapshot_diff(const BasisSnapshot *s1, const BasisSnapshot *s2);

/**
 * @brief Check if two snapshots are identical.
 *
 * @param s1 First snapshot.
 * @param s2 Second snapshot.
 * @return 1 if equal, 0 if different.
 */
int cxf_basis_snapshot_equal(const BasisSnapshot *s1, const BasisSnapshot *s2);

/**
 * @brief Free memory allocated within a snapshot.
 *
 * @param snapshot Snapshot to free (does not free the struct itself).
 */
void cxf_basis_snapshot_free(BasisSnapshot *snapshot);

#endif /* CXF_BASIS_H */
