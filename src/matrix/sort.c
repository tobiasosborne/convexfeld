/**
 * @file sort.c
 * @brief Sort sparse matrix indices (M4.1.6)
 *
 * Sorts arrays of integer indices with optional value synchronization.
 * Uses introsort (Musser 1997): quicksort with median-of-three pivot,
 * heapsort fallback at depth 2*log2(n), insertion sort for n <= 16.
 *
 * Spec: docs/specs/functions/matrix/cxf_sort_indices.md
 */

#include <stdlib.h>
#include "convexfeld/cxf_types.h"

#define INTROSORT_THRESHOLD 16

static void swap_elem(int *indices, double *values, int a, int b) {
    int ti = indices[a]; indices[a] = indices[b]; indices[b] = ti;
    if (values) {
        double tv = values[a]; values[a] = values[b]; values[b] = tv;
    }
}

/*============================================================================
 * Insertion sort — base case for small partitions
 *===========================================================================*/

static void insertion_sort(int *indices, double *values, int n) {
    for (int i = 1; i < n; i++) {
        int key_idx = indices[i];
        double key_val = values ? values[i] : 0.0;
        int j = i - 1;
        while (j >= 0 && indices[j] > key_idx) {
            indices[j + 1] = indices[j];
            if (values) values[j + 1] = values[j];
            j--;
        }
        indices[j + 1] = key_idx;
        if (values) values[j + 1] = key_val;
    }
}

/*============================================================================
 * Heapsort — fallback when recursion depth limit is reached
 *===========================================================================*/

static void sift_down(int *indices, double *values, int start, int end) {
    int root = start;
    while (2 * root + 1 <= end) {
        int child = 2 * root + 1;
        if (child + 1 <= end && indices[child] < indices[child + 1])
            child++;
        if (indices[root] < indices[child]) {
            swap_elem(indices, values, root, child);
            root = child;
        } else {
            return;
        }
    }
}

static void heapsort(int *indices, double *values, int n) {
    /* Build max-heap */
    for (int i = (n - 2) / 2; i >= 0; i--)
        sift_down(indices, values, i, n - 1);
    /* Extract elements */
    for (int end = n - 1; end > 0; end--) {
        swap_elem(indices, values, 0, end);
        sift_down(indices, values, 0, end - 1);
    }
}

/*============================================================================
 * Floor log2 — compute depth limit
 *===========================================================================*/

static int floor_log2(int n) {
    int k = 0;
    while (n > 1) { n >>= 1; k++; }
    return k;
}

/*============================================================================
 * Median-of-three pivot selection
 *===========================================================================*/

static int median3(int *indices, int lo, int mid, int hi) {
    int a = indices[lo], b = indices[mid], c = indices[hi];
    if (a <= b) {
        if (b <= c) return mid;      /* a <= b <= c */
        return (a <= c) ? hi : lo;   /* a <= c < b  or  c < a <= b */
    }
    /* b < a */
    if (a <= c) return lo;           /* b < a <= c */
    return (b <= c) ? hi : mid;      /* b <= c < a  or  c < b < a */
}

/*============================================================================
 * Introsort recursive implementation
 *===========================================================================*/

static void introsort_impl(int *indices, double *values,
                           int lo, int hi, int depth) {
    while (lo < hi) {
        int size = hi - lo + 1;

        if (size <= INTROSORT_THRESHOLD) {
            insertion_sort(indices + lo, values ? values + lo : NULL, size);
            return;
        }

        if (depth == 0) {
            heapsort(indices + lo, values ? values + lo : NULL, size);
            return;
        }

        /* Median-of-three pivot */
        int mid = lo + (hi - lo) / 2;
        int pivot_pos = median3(indices, lo, mid, hi);
        int pivot = indices[pivot_pos];

        /* Hoare partition */
        int i = lo - 1, j = hi + 1;
        for (;;) {
            do { i++; } while (indices[i] < pivot);
            do { j--; } while (indices[j] > pivot);
            if (i >= j) break;
            swap_elem(indices, values, i, j);
        }

        /* Tail recursion: recurse on smaller partition, iterate on larger */
        depth--;
        if (j - lo < hi - j) {
            introsort_impl(indices, values, lo, j, depth);
            lo = j + 1;
        } else {
            introsort_impl(indices, values, j + 1, hi, depth);
            hi = j;
        }
    }
}

/*============================================================================
 * cxf_sort_by_values — Indices only
 *===========================================================================*/

/**
 * @brief Sort an array of indices in ascending order using introsort.
 */
void cxf_sort_by_values(int *indices, int n) {
    if (indices == NULL || n <= 1) return;
    int depth = 2 * floor_log2(n);
    introsort_impl(indices, NULL, 0, n - 1, depth);
}

/*============================================================================
 * cxf_sort_by_values_paired — Indices with synchronized values
 *===========================================================================*/

/**
 * @brief Sort indices with synchronized value array using introsort.
 */
void cxf_sort_by_values_paired(int *indices, double *values, int n) {
    if (indices == NULL || values == NULL || n <= 1) return;
    int depth = 2 * floor_log2(n);
    introsort_impl(indices, values, 0, n - 1, depth);
}
