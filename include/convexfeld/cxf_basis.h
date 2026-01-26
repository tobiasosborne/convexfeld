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
    int nnz;                  /**< Non-zeros in eta vector */
    int *indices;             /**< Row indices [nnz] */
    double *values;           /**< Values [nnz] */
    double pivot_elem;        /**< Pivot element */
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
};

#endif /* CXF_BASIS_H */
