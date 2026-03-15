/**
 * @file sort_values.c
 * @brief cxf_sort_indices — sort values ascending, indices as satellite
 *
 * V2 spec: docs/specs-v2/specs/modules/matrix_core.md §cxf_sort_indices
 * Primary sort key is the double values array. The integer indices array
 * is permuted identically. Uses introsort (Musser 1997).
 */

#include <stdint.h>
#include "convexfeld/cxf_types.h"

#define INTROSORT_THRESHOLD 16

static void swap_val(double *values, int *indices, int a, int b) {
    double tv = values[a]; values[a] = values[b]; values[b] = tv;
    int ti = indices[a]; indices[a] = indices[b]; indices[b] = ti;
}

/*============================================================================
 * Insertion sort — base case for small partitions (keyed on values)
 *===========================================================================*/

static void isort_val(double *values, int *indices, int n) {
    for (int i = 1; i < n; i++) {
        double kv = values[i];
        int ki = indices[i];
        int j = i - 1;
        while (j >= 0 && values[j] > kv) {
            values[j + 1] = values[j];
            indices[j + 1] = indices[j];
            j--;
        }
        values[j + 1] = kv;
        indices[j + 1] = ki;
    }
}

/*============================================================================
 * Heapsort fallback (keyed on values)
 *===========================================================================*/

static void sift_val(double *values, int *indices, int start, int end) {
    int root = start;
    while (2 * root + 1 <= end) {
        int child = 2 * root + 1;
        if (child + 1 <= end && values[child] < values[child + 1])
            child++;
        if (values[root] < values[child]) {
            swap_val(values, indices, root, child);
            root = child;
        } else {
            return;
        }
    }
}

static void hsort_val(double *values, int *indices, int n) {
    for (int i = (n - 2) / 2; i >= 0; i--)
        sift_val(values, indices, i, n - 1);
    for (int end = n - 1; end > 0; end--) {
        swap_val(values, indices, 0, end);
        sift_val(values, indices, 0, end - 1);
    }
}

/*============================================================================
 * Floor log2
 *===========================================================================*/

static int flog2(int n) {
    int k = 0;
    while (n > 1) { n >>= 1; k++; }
    return k;
}

/*============================================================================
 * Median-of-three (keyed on values)
 *===========================================================================*/

static int med3_val(double *values, int lo, int mid, int hi) {
    double a = values[lo], b = values[mid], c = values[hi];
    if (a <= b) {
        if (b <= c) return mid;
        return (a <= c) ? hi : lo;
    }
    if (a <= c) return lo;
    return (b <= c) ? hi : mid;
}

/*============================================================================
 * Introsort — keyed on values (doubles ascending)
 *===========================================================================*/

static void introsort_val(double *values, int *indices,
                          int lo, int hi, int depth) {
    while (lo < hi) {
        int size = hi - lo + 1;

        if (size <= INTROSORT_THRESHOLD) {
            isort_val(values + lo, indices + lo, size);
            return;
        }
        if (depth == 0) {
            hsort_val(values + lo, indices + lo, size);
            return;
        }

        int mid = lo + (hi - lo) / 2;
        int pp = med3_val(values, lo, mid, hi);
        double pivot = values[pp];

        /* Hoare partition on values */
        int i = lo - 1, j = hi + 1;
        for (;;) {
            do { i++; } while (values[i] < pivot);
            do { j--; } while (values[j] > pivot);
            if (i >= j) break;
            swap_val(values, indices, i, j);
        }

        depth--;
        if (j - lo < hi - j) {
            introsort_val(values, indices, lo, j, depth);
            lo = j + 1;
        } else {
            introsort_val(values, indices, j + 1, hi, depth);
            hi = j;
        }
    }
}

/*============================================================================
 * cxf_sort_indices — V2 spec public API
 *
 * Sort values ascending, permuting indices as satellite.
 * Signature: (int64_t count, double *values, int *indices)
 *===========================================================================*/

/**
 * @brief Sort values ascending with synchronized index array.
 *
 * Per V2 spec (matrix_core.md §cxf_sort_indices): primary sort key is
 * the values array (doubles, ascending). The indices array is permuted
 * identically to maintain value-index correspondence.
 */
void cxf_sort_indices(int64_t count, double *values, int *indices) {
    if (count <= 1 || values == NULL || indices == NULL) return;
    int n = (int)count;
    int depth = 2 * flog2(n);
    introsort_val(values, indices, 0, n - 1, depth);
}
