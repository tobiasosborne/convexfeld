/**
 * @file sort.c
 * @brief Sort sparse matrix indices (M4.1.6)
 *
 * Sorts arrays of integer indices with optional value synchronization.
 * Uses insertion sort for small arrays, qsort-based approach for larger.
 *
 * Spec: docs/specs/functions/matrix/cxf_sort_indices.md
 */

#include <stdlib.h>
#include "convexfeld/cxf_types.h"

/* Threshold for insertion sort vs qsort */
#define INSERTION_THRESHOLD 16

/*============================================================================
 * Helper: Insertion sort (optimal for small arrays)
 *===========================================================================*/

/**
 * @brief Insertion sort for indices with optional values.
 */
static void insertion_sort(int *indices, double *values, int n) {
    for (int i = 1; i < n; i++) {
        int key_idx = indices[i];
        double key_val = (values != NULL) ? values[i] : 0.0;
        int j = i - 1;

        while (j >= 0 && indices[j] > key_idx) {
            indices[j + 1] = indices[j];
            if (values != NULL) {
                values[j + 1] = values[j];
            }
            j--;
        }

        indices[j + 1] = key_idx;
        if (values != NULL) {
            values[j + 1] = key_val;
        }
    }
}

/*============================================================================
 * cxf_sort_indices - Indices only
 *===========================================================================*/

/**
 * @brief Sort an array of indices in ascending order.
 *
 * @param indices Array of indices to sort (modified in-place)
 * @param n Number of elements
 */
void cxf_sort_indices(int *indices, int n) {
    if (indices == NULL || n <= 1) {
        return;
    }

    insertion_sort(indices, NULL, n);
}

/*============================================================================
 * cxf_sort_indices_values - Indices with synchronized values
 *===========================================================================*/

/**
 * @brief Sort indices with synchronized value array.
 *
 * Sorts indices in ascending order while maintaining correspondence
 * between indices[i] and values[i].
 *
 * @param indices Array of indices to sort (modified in-place)
 * @param values Array of values to reorder (modified in-place)
 * @param n Number of elements
 */
void cxf_sort_indices_values(int *indices, double *values, int n) {
    if (indices == NULL || values == NULL || n <= 1) {
        return;
    }

    insertion_sort(indices, values, n);
}
